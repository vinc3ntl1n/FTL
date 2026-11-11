// Runs the FTL on the emulator or on the real chip, checks every read against the reference model,
// and prints the write amplification and the erase counts. It also saves every block's erase count
// to a CSV file, for tools/heatmap.py.
//
//   build/dev/ftl_run                                    emulator, 32 blocks, 100,000 writes
//   build/dev/ftl_run --hot-cold                         most writes go to a few pages
//   build/dev/ftl_run --chip --bad-blocks bad-blocks-chip-1.txt
//
// Options:
//   --chip              use the real chip (on the Pi) instead of the emulator
//   --blocks N          how many blocks to use (default 32)
//   --writes N          random writes after the first full pass (default 100,000; 2,000 on the
//   chip)
//   --hot-cold          send 9 of every 10 writes to the first tenth of the pages
//   --seed N            change the random writes (default 1)
//   --bad-blocks FILE   the chip's bad-block list from tools/pi/scan_bad_blocks.py (--chip only)
//   --speed HZ          SPI clock (default 1,000,000; try 8,000,000 once the chip works)
//   --csv FILE          where to save the erase counts (default erase-counts.csv)

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "vincentftl/linux_spi_bus.hpp"
#include "vincentftl/nand_emulator.hpp"
#include "vincentftl/page_ftl.hpp"
#include "vincentftl/reference_ftl.hpp"
#include "vincentftl/w25n_device.hpp"

namespace vincentftl {
namespace {

struct Options {
  bool chip = false;
  std::uint32_t blocks = 32;
  std::uint32_t writes = 0;  // 0 means the default for the device
  bool hot_cold = false;
  std::uint32_t seed = 1;
  std::string bad_blocks;
  std::uint32_t speed_hz = 1'000'000;
  std::string csv = "erase-counts.csv";
};

[[noreturn]] void usage(const std::string& problem) {
  std::cerr << "ftl_run: " << problem
            << "\n(the options are listed at the top of tools/ftl_run.cpp)\n";
  std::exit(2);
}

Options parse(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto value = [&]() -> std::string {
      if (i + 1 >= argc) {
        usage(arg + " needs a value");
      }
      return argv[++i];
    };
    auto number = [&]() -> std::uint32_t {
      const std::string text = value();
      try {
        return static_cast<std::uint32_t>(std::stoul(text));
      } catch (const std::exception&) {
        usage(arg + " needs a number, not " + text);
      }
    };
    if (arg == "--chip") {
      options.chip = true;
    } else if (arg == "--blocks") {
      options.blocks = number();
    } else if (arg == "--writes") {
      options.writes = number();
    } else if (arg == "--hot-cold") {
      options.hot_cold = true;
    } else if (arg == "--seed") {
      options.seed = number();
    } else if (arg == "--bad-blocks") {
      options.bad_blocks = value();
    } else if (arg == "--speed") {
      options.speed_hz = number();
    } else if (arg == "--csv") {
      options.csv = value();
    } else {
      usage("unknown option " + arg);
    }
  }
  if (options.writes == 0) {
    options.writes = options.chip ? 2'000 : 100'000;
  }
  return options;
}

// Passes everything through to another device, counting programs and erases on the way.
class CountingDevice final : public NandDevice {
 public:
  explicit CountingDevice(NandDevice& inner) : inner_(inner), erases_(inner.block_count(), 0) {}

  std::uint32_t block_count() const override { return inner_.block_count(); }
  Status read_page(PhysicalPage page, PageData& data, PageSpare& spare) override {
    return inner_.read_page(page, data, spare);
  }
  Status read_spare(PhysicalPage page, PageSpare& spare) override {
    return inner_.read_spare(page, spare);
  }
  Status program_page(PhysicalPage page, const PageData& data, const PageSpare& spare) override {
    const Status status = inner_.program_page(page, data, spare);
    if (status == Status::kOk) {
      ++programs_;
    }
    return status;
  }
  Status erase_block(BlockNumber block) override {
    const Status status = inner_.erase_block(block);
    if (status == Status::kOk) {
      ++erases_[block.value];
    }
    return status;
  }
  bool is_factory_bad(BlockNumber block) override { return inner_.is_factory_bad(block); }

  std::uint64_t programs() const { return programs_; }
  std::uint32_t erases(BlockNumber block) const { return erases_[block.value]; }

 private:
  NandDevice& inner_;
  std::uint64_t programs_ = 0;
  std::vector<std::uint32_t> erases_;
};

// Different for every logical page and every version, so reading a wrong copy shows up.
PageData pattern(std::uint32_t page, std::uint32_t version) {
  PageData data;
  data.fill(static_cast<std::uint8_t>(version * 31 + page));
  for (int i = 0; i < 4; ++i) {
    data[i] = static_cast<std::uint8_t>(page >> (8 * i));
    data[4 + i] = static_cast<std::uint8_t>(version >> (8 * i));
  }
  return data;
}

// Erases every good block, so the chip starts blank. The emulator always starts blank; the real
// chip keeps whatever the last run left on it.
bool erase_everything(W25nDevice& chip) {
  for (std::uint32_t b = 0; b < chip.block_count(); ++b) {
    const BlockNumber block{b};
    if (!chip.is_factory_bad(block) && chip.erase_block(block) != Status::kOk) {
      std::cerr << "Couldn't erase block " << b << " of the chip.\n";
      return false;
    }
  }
  return true;
}

int run(const Options& options, NandDevice& device) {
  CountingDevice counted(device);
  PageFtl ftl(counted);
  ReferenceFtl reference(ftl.capacity());
  if (ftl.capacity() == 0) {
    std::cerr << "The FTL has no room on " << options.blocks << " blocks. Use more.\n";
    return 1;
  }

  std::mt19937 rng(options.seed);
  const std::uint32_t capacity = ftl.capacity();
  const std::uint32_t hot_pages = capacity / 10 > 0 ? capacity / 10 : 1;
  const std::uint32_t total = capacity + options.writes;
  std::vector<std::uint32_t> version(capacity, 0);
  std::uint32_t mismatches = 0;

  auto check = [&](std::uint32_t page) {
    PageData got;
    PageData want;
    const FtlStatus status = ftl.read(LogicalPage{page}, got);
    reference.read(LogicalPage{page}, want);
    if (status != FtlStatus::kOk || got != want) {
      if (++mismatches <= 5) {
        std::cerr << "  Logical page " << page << " read back wrong (" << status << ")\n";
      }
    }
  };

  std::cout << "Writing every one of the FTL's " << capacity << " pages once, then "
            << options.writes << (options.hot_cold ? " hot/cold" : " random") << " writes...\n";
  const auto started = std::chrono::steady_clock::now();
  for (std::uint32_t i = 0; i < total; ++i) {
    std::uint32_t page = i;
    if (i >= capacity) {
      const bool hot = options.hot_cold && rng() % 10 != 0;
      page = static_cast<std::uint32_t>(hot ? rng() % hot_pages : rng() % capacity);
    }
    const PageData data = pattern(page, ++version[page]);
    const FtlStatus status = ftl.write(LogicalPage{page}, data);
    if (status != FtlStatus::kOk) {
      std::cerr << "Write " << i << " (logical page " << page << ") failed: " << status << "\n";
      return 1;
    }
    reference.write(LogicalPage{page}, data);
    check(static_cast<std::uint32_t>(rng() % (i < capacity ? i + 1 : capacity)));
    if (options.chip && (i + 1) % (total / 10 > 0 ? total / 10 : 1) == 0) {
      std::cout << "  " << (i + 1) * 100 / total << "%\n";
    }
  }
  for (std::uint32_t page = 0; page < capacity; ++page) {
    check(page);
  }
  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

  std::uint32_t least = UINT32_MAX;
  std::uint32_t most = 0;
  std::uint64_t erases = 0;
  std::ofstream csv(options.csv);
  csv << "block,erase_count,factory_bad\n";
  for (std::uint32_t b = 0; b < device.block_count(); ++b) {
    const BlockNumber block{b};
    const bool bad = device.is_factory_bad(block);
    const std::uint32_t count = counted.erases(block);
    csv << b << ',' << count << ',' << (bad ? 1 : 0) << '\n';
    if (!bad) {
      least = std::min(least, count);
      most = std::max(most, count);
      erases += count;
    }
  }

  std::cout << "\nMismatches:            " << mismatches << (mismatches == 0 ? "  (correct)" : "")
            << "\nHost writes:           " << total
            << "\nPages programmed:      " << counted.programs()
            << "\nWrite amplification:   " << static_cast<double>(counted.programs()) / total
            << "\nBlocks erased:         " << erases << "\nErases per block:      " << least
            << " (least worn) to " << most << " (most worn)\nTook:                  " << seconds
            << " s"
            << "\nErase counts saved to: " << options.csv << "\n";
  return mismatches == 0 ? 0 : 1;
}

int run_on_chip(const Options& options) {
  std::vector<BlockNumber> bad;
  if (options.bad_blocks.empty()) {
    std::cerr << "Warning: no --bad-blocks list, so every block is treated as good.\n";
  } else {
    std::ifstream in(options.bad_blocks);
    if (!in) {
      usage("can't read " + options.bad_blocks);
    }
    bad = read_bad_block_list(in);
  }
  LinuxSpiBus bus("/dev/spidev0.0", options.speed_hz);
  W25nDevice chip(bus, options.blocks, bad);
  if (!chip.start()) {
    std::cerr << "The chip doesn't answer. Try python3 tools/pi/chip_id.py.\n";
    return 1;
  }
  std::cout << "Erasing the first " << options.blocks << " blocks of the chip...\n";
  if (!erase_everything(chip)) {
    return 1;
  }
  return run(options, chip);
}

}  // namespace
}  // namespace vincentftl

int main(int argc, char** argv) {
  using namespace vincentftl;
  const Options options = parse(argc, argv);
  if (options.blocks == 0 || options.blocks > kBlockCount) {
    usage("--blocks must be from 1 to 1024");
  }
  try {
    if (options.chip) {
      return run_on_chip(options);
    }
    NandEmulator emulator(options.blocks);
    return run(options, emulator);
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
