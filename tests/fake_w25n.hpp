#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "vincentftl/geometry.hpp"
#include "vincentftl/nand_emulator.hpp"
#include "vincentftl/spi_bus.hpp"

namespace vincentftl {

// A pretend W25N01GV at the other end of the SPI wires, for testing W25nDevice without a Pi. It
// answers the commands the driver needs, byte for byte as the datasheet describes them, and keeps
// its pages in a NandEmulator.
//
// It is stricter than the real chip. When the driver sends something the real chip would ignore,
// or would get wrong without saying so, the fake writes a sentence in mistakes(). Tests check
// that the list stays empty.
//
// Like the real chip after a program, page read or erase, it reports busy for the next few status
// reads, so a driver that doesn't wait shows up in mistakes().
class FakeW25n final : public SpiBus {
 public:
  explicit FakeW25n(std::uint32_t block_count = 8, const std::vector<BlockNumber>& factory_bad = {})
      : chip_(block_count, factory_bad) {
    buffer_.fill(kErasedByte);
  }

  // The pages behind the pretend chip. Tests use it to put data on the chip, or check what's there.
  NandEmulator& chip() { return chip_; }

  // What the driver did that the real chip wouldn't accept. Empty when the driver is right.
  const std::vector<std::string>& mistakes() const { return mistakes_; }

  // The first byte of every transfer so far: the commands the driver sent, in order.
  const std::vector<std::uint8_t>& commands() const { return commands_; }

  // From now on the chip never stops being busy, to test what the driver does about that.
  void stay_busy() { stuck_busy_ = true; }

  std::vector<std::uint8_t> transfer(const std::vector<std::uint8_t>& tx) override {
    std::vector<std::uint8_t> rx(tx.size(), kErasedByte);
    if (tx.empty()) {
      mistake("an empty transfer");
      return rx;
    }
    const std::uint8_t command = tx[0];
    commands_.push_back(command);

    const bool busy = stuck_busy_ || busy_polls_left_ > 0;
    const bool allowed_while_busy = command == 0x0F || command == 0x05 || command == 0x9F;
    if (busy && !allowed_while_busy) {
      mistake(hex(command) +
              " was sent while the chip was busy, so the real chip ignores it. Call " +
              "wait_ready() first");
      return rx;
    }

    switch (command) {
      case 0xFF:  // Device Reset (8.2.1)
        expect_length(tx, 1, "Device Reset (FFh)");
        protection_ = kPowerUpProtection;
        config_ = kPowerUpConfig;
        status_ = 0;
        break;
      case 0x9F:  // Read JEDEC ID (8.2.2)
        expect_length(tx, 5, "Read JEDEC ID (9Fh): opcode, 1 dummy byte, 3 ID bytes,");
        reply(rx, 2, {0xEF, 0xAA, 0x21});
        break;
      case 0x0F:  // Read Status Register (8.2.3)
      case 0x05:
        read_register(tx, rx, busy);
        break;
      case 0x1F:  // Write Status Register (8.2.4)
      case 0x01:
        write_register(tx);
        break;
      case 0x06:  // Write Enable (8.2.5)
        expect_length(tx, 1, "Write Enable (06h)");
        status_ |= kWriteEnabled;
        break;
      case 0x04:  // Write Disable (8.2.6)
        status_ &= ~kWriteEnabled;
        break;
      case 0x13:  // Page Data Read (8.2.14)
        page_data_read(tx);
        break;
      case 0x03:  // Read Data, buffer mode (8.2.15)
        read_data(tx, rx);
        break;
      case 0x02:  // Load Program Data (8.2.11)
      case 0x84:  // Random Load Program Data
        load_program_data(tx);
        break;
      case 0x10:  // Program Execute (8.2.13)
        program_execute(tx);
        break;
      case 0xD8:  // Block Erase (8.2.10)
        block_erase(tx);
        break;
      default:
        mistake(hex(command) + " is a command the fake doesn't know. Check the datasheet's " +
                "instruction table (8.1.3)");
    }
    return rx;
  }

 private:
  static constexpr std::uint8_t kPowerUpProtection = 0x7C;  // every block protected
  static constexpr std::uint8_t kPowerUpConfig = 0x18;      // ECC on, buffer read mode
  static constexpr std::uint8_t kBlockProtectBits = 0x78;   // BP3 to BP0 (7.1.1)
  static constexpr std::uint8_t kBusyBit = 0x01;
  static constexpr std::uint8_t kWriteEnabled = 0x02;
  static constexpr std::uint8_t kEraseFailed = 0x04;
  static constexpr std::uint8_t kProgramFailed = 0x08;
  static constexpr std::uint32_t kBusyPolls = 2;
  static constexpr std::size_t kBufferBytes = kPageDataBytes + kPageSpareBytes;

  static std::string hex(std::uint32_t value) {
    char text[8];
    std::snprintf(text, sizeof text, "%02Xh", static_cast<unsigned>(value));
    return text;
  }

  void mistake(const std::string& what) { mistakes_.push_back(what); }

  void expect_length(const std::vector<std::uint8_t>& tx, std::size_t want,
                     const std::string& what) {
    if (tx.size() != want) {
      mistake(what + " should be " + std::to_string(want) + " bytes long, but was " +
              std::to_string(tx.size()));
    }
  }

  static void reply(std::vector<std::uint8_t>& rx, std::size_t at,
                    const std::vector<std::uint8_t>& bytes) {
    for (std::size_t i = 0; i < bytes.size() && at + i < rx.size(); ++i) {
      rx[at + i] = bytes[i];
    }
  }

  // Page Data Read, Program Execute and Block Erase all send: opcode, 1 dummy byte, then the
  // 16-bit page address, high byte first.
  std::uint32_t page_address(const std::vector<std::uint8_t>& tx, const std::string& what) {
    if (tx.size() != 4) {
      mistake(what +
              " should be 4 bytes long (opcode, dummy byte, 2 page address bytes), but was " +
              std::to_string(tx.size()));
      return 0;
    }
    return (std::uint32_t{tx[2]} << 8) | tx[3];
  }

  void start_busy() { busy_polls_left_ = kBusyPolls; }

  void read_register(const std::vector<std::uint8_t>& tx, std::vector<std::uint8_t>& rx,
                     bool busy) {
    if (tx.size() < 3) {
      mistake("Read Status Register (0Fh) needs at least 3 bytes: opcode, address, 1 byte back");
      return;
    }
    std::uint8_t value = 0;
    switch (tx[1]) {
      case 0xA0:
        value = protection_;
        break;
      case 0xB0:
        value = config_;
        break;
      case 0xC0:
        value = static_cast<std::uint8_t>(status_ | (busy ? kBusyBit : 0));
        if (busy_polls_left_ > 0) {
          --busy_polls_left_;
        }
        break;
      default:
        mistake("Read Status Register asked for register " + hex(tx[1]) +
                ". The registers are A0h, B0h and C0h");
        return;
    }
    for (std::size_t i = 2; i < rx.size(); ++i) {
      rx[i] = value;
    }
  }

  void write_register(const std::vector<std::uint8_t>& tx) {
    expect_length(tx, 3, "Write Status Register (1Fh)");
    if (tx.size() < 3) {
      return;
    }
    switch (tx[1]) {
      case 0xA0:
        protection_ = tx[2];
        break;
      case 0xB0:
        if ((tx[2] & 0x08) == 0) {
          mistake("Write Status Register turned buffer read mode off (BUF = 0). Keep it on");
        }
        config_ = static_cast<std::uint8_t>(tx[2] & 0xF8);
        break;
      default:
        mistake("Write Status Register wrote to register " + hex(tx[1]) +
                ". Only A0h and B0h can be written");
    }
  }

  void page_data_read(const std::vector<std::uint8_t>& tx) {
    const std::uint32_t page = page_address(tx, "Page Data Read (13h)");
    buffer_.fill(kErasedByte);
    if (page >= chip_.block_count() * kPagesPerBlock) {
      mistake("Page Data Read of page " + std::to_string(page) + ", past the end of the chip");
    } else {
      PageData data;
      PageSpare spare;
      chip_.read_page(PhysicalPage{page}, data, spare);
      std::copy(data.begin(), data.end(), buffer_.begin());
      std::copy(spare.begin(), spare.end(), buffer_.begin() + kPageDataBytes);
    }
    start_busy();
  }

  void read_data(const std::vector<std::uint8_t>& tx, std::vector<std::uint8_t>& rx) {
    if (tx.size() < 5) {
      mistake(
          "Read Data (03h) needs opcode, 2 column address bytes, 1 dummy byte, then the bytes "
          "to read");
      return;
    }
    const std::size_t column = (std::size_t{tx[1]} << 8) | tx[2];
    for (std::size_t i = 4; i < rx.size(); ++i) {
      const std::size_t at = column + (i - 4);
      rx[i] = at < kBufferBytes ? buffer_[at] : kErasedByte;
    }
  }

  void load_program_data(const std::vector<std::uint8_t>& tx) {
    if ((status_ & kWriteEnabled) == 0) {
      mistake(
          "Load Program Data (02h) was sent without Write Enable (06h) first, so the real "
          "chip ignores it");
      return;
    }
    if (tx.size() < 4) {
      mistake("Load Program Data (02h) needs opcode, 2 column address bytes, then the data");
      return;
    }
    if (tx[0] == 0x02) {
      buffer_.fill(kErasedByte);
    }
    const std::size_t column = ((std::size_t{tx[1]} << 8) | tx[2]) & 0x0FFF;
    for (std::size_t i = 3; i < tx.size(); ++i) {
      const std::size_t at = column + (i - 3);
      if (at < kBufferBytes) {
        buffer_[at] = tx[i];
      }
    }
  }

  void program_execute(const std::vector<std::uint8_t>& tx) {
    const std::uint32_t page = page_address(tx, "Program Execute (10h)");
    if ((status_ & kWriteEnabled) == 0) {
      mistake(
          "Program Execute (10h) was sent without Write Enable (06h) first, so the real chip "
          "ignores it");
      return;
    }
    status_ &= ~(kProgramFailed | kWriteEnabled);
    start_busy();
    if ((protection_ & kBlockProtectBits) != 0) {
      status_ |= kProgramFailed;  // the chip is still write-protected (7.3.3)
      return;
    }
    if (page >= chip_.block_count() * kPagesPerBlock) {
      mistake("Program Execute of page " + std::to_string(page) + ", past the end of the chip");
      status_ |= kProgramFailed;
      return;
    }
    const PhysicalPage target{page};
    if (chip_.is_factory_bad(target.block())) {
      mistake("Program Execute into factory bad block " + std::to_string(target.block().value) +
              ". Never program a factory bad block");
      status_ |= kProgramFailed;
      return;
    }
    PageData data;
    PageSpare spare;
    std::copy(buffer_.begin(), buffer_.begin() + kPageDataBytes, data.begin());
    std::copy(buffer_.begin() + kPageDataBytes, buffer_.end(), spare.begin());
    for (std::size_t i = 0; i < kPageSpareBytes; ++i) {
      if (is_reserved_spare_byte(i)) {
        spare[i] = kErasedByte;  // the real chip puts its ECC bytes here (8.2.11)
      }
    }
    const Status result = chip_.program_page(target, data, spare);
    if (result != Status::kOk) {
      mistake("Program Execute of page " + std::to_string(page) + " broke a NAND rule (" +
              std::string(to_string(result)) + "). The real chip wouldn't say so, and the data " +
              "would be wrong");
      status_ |= kProgramFailed;
    }
  }

  void block_erase(const std::vector<std::uint8_t>& tx) {
    const std::uint32_t page = page_address(tx, "Block Erase (D8h)");
    if ((status_ & kWriteEnabled) == 0) {
      mistake(
          "Block Erase (D8h) was sent without Write Enable (06h) first, so the real chip "
          "ignores it");
      return;
    }
    status_ &= ~(kEraseFailed | kWriteEnabled);
    start_busy();
    if ((protection_ & kBlockProtectBits) != 0) {
      status_ |= kEraseFailed;  // the chip is still write-protected (7.3.4)
      return;
    }
    const BlockNumber block = PhysicalPage{page}.block();
    if (block.value >= chip_.block_count()) {
      mistake("Block Erase of block " + std::to_string(block.value) + ", past the end of the chip");
      status_ |= kEraseFailed;
      return;
    }
    if (chip_.is_factory_bad(block)) {
      mistake("Block Erase of factory bad block " + std::to_string(block.value) +
              ". On the real chip that wipes its bad-block mark for good (10.2)");
      status_ |= kEraseFailed;
      return;
    }
    if (chip_.erase_block(block) != Status::kOk) {
      status_ |= kEraseFailed;
    }
  }

  NandEmulator chip_;
  std::array<std::uint8_t, kBufferBytes> buffer_{};
  std::uint8_t protection_ = kPowerUpProtection;
  std::uint8_t config_ = kPowerUpConfig;
  std::uint8_t status_ = 0;
  std::uint32_t busy_polls_left_ = 0;
  bool stuck_busy_ = false;
  std::vector<std::string> mistakes_;
  std::vector<std::uint8_t> commands_;
};

}  // namespace vincentftl
