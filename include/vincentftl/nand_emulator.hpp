#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "vincentftl/geometry.hpp"
#include "vincentftl/nand_device.hpp"

namespace vincentftl {

class NandEmulator final : public NandDevice {
 public:
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

  std::uint32_t erase_count(BlockNumber block) const;
  std::uint64_t pages_programmed() const { return pages_programmed_; }
  std::uint64_t blocks_erased() const { return blocks_erased_; }

 private:
  struct StoredPage {
    PageData data;
    PageSpare spare;
  };

  struct BlockState {
    bool factory_bad = false;
    std::uint32_t next_page = 0;
    std::uint32_t erase_count = 0;
  };

  bool in_range(PhysicalPage page) const;
  bool in_range(BlockNumber block) const;

  std::vector<BlockState> blocks_;
  std::vector<std::unique_ptr<StoredPage>> pages_;
  std::uint64_t pages_programmed_ = 0;
  std::uint64_t blocks_erased_ = 0;
};

}  // namespace vincentftl
