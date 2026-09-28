#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "vincentftl/geometry.hpp"
#include "vincentftl/nand_device.hpp"

namespace vincentftl {

// A W25N01GV held in memory. It enforces the rules in docs/nand-rules.md, and it counts every
// program and erase so tests can check what the FTL did to the chip.
class NandEmulator final : public NandDevice {
 public:
  // A device with `block_count` blocks (1 to kBlockCount). Every byte starts erased (0xFF),
  // except that each block in `factory_bad_blocks` carries the factory's bad-block marker.
  // Throws std::invalid_argument for a block count or bad block the chip couldn't have, including
  // block 0, which the chip guarantees is good.
  explicit NandEmulator(std::uint32_t block_count = kBlockCount,
                        const std::vector<BlockNumber>& factory_bad_blocks = {});

  NandEmulator(const NandEmulator&) = delete;
  NandEmulator& operator=(const NandEmulator&) = delete;

  std::uint32_t block_count() const override;
  Status read_page(PhysicalPage page, PageData& data, PageSpare& spare) override;
  Status read_spare(PhysicalPage page, PageSpare& spare) override;
  Status program_page(PhysicalPage page, const PageData& data, const PageSpare& spare) override;
  Status erase_block(BlockNumber block) override;
  bool is_factory_bad(BlockNumber block) override;

  // Counters. Only programs and erases that return kOk are counted.

  // How many times `block` has been erased. Throws std::out_of_range past the end of the device.
  std::uint32_t erase_count(BlockNumber block) const;
  // Totals for the whole device. pages_programmed() is the chip's side of write amplification.
  std::uint64_t pages_programmed() const { return pages_programmed_; }
  std::uint64_t blocks_erased() const { return blocks_erased_; }

 private:
  struct StoredPage {
    PageData data;
    PageSpare spare;
  };

  struct BlockState {
    bool factory_bad = false;
    // Programs must go to this page or higher: one past the highest page programmed since the
    // last erase.
    std::uint32_t next_page = 0;
    std::uint32_t erase_count = 0;
  };

  bool in_range(PhysicalPage page) const;
  bool in_range(BlockNumber block) const;

  std::vector<BlockState> blocks_;
  // One entry per page, and nullptr means erased. A page only takes memory once it's programmed,
  // which keeps tests fast: a full chip stored up front would be 138 MB.
  std::vector<std::unique_ptr<StoredPage>> pages_;
  std::uint64_t pages_programmed_ = 0;
  std::uint64_t blocks_erased_ = 0;
};

}  // namespace vincentftl
