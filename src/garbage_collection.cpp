#include "vincentftl/garbage_collection.hpp"

namespace vincentftl {

FtlStatus collect_block(NandDevice& device, BlockManager& blocks, BlockNumber victim,
                        std::vector<PageMove>& moves) {
  for (std::uint32_t n = 0; n < kPagesPerBlock; ++n) {
    const PhysicalPage from = PhysicalPage::in_block(victim, n);

    if (!blocks.is_valid(from)) {
      continue;
    }

    PageData data;
    PageSpare spare;
    if (device.read_page(from, data, spare) != Status::kOk) {
      return FtlStatus::kDeviceError;
    }

    // The chip owns the reserved spare bytes, so the copy only carries over the FTL's bytes.
    PageSpare copy = erased_spare();
    for (std::size_t i = 0; i < kPageSpareBytes; ++i) {
      if (!is_reserved_spare_byte(i)) {
        copy[i] = spare[i];
      }
    }

    const auto to = blocks.allocate_page();
    if (!to) {
      return FtlStatus::kNoSpace;
    }

    if (device.program_page(*to, data, copy) != Status::kOk) {
      blocks.mark_invalid(*to);
      return FtlStatus::kDeviceError;
    }

    blocks.mark_invalid(from);
    moves.push_back({from, *to});
  }

  if (device.erase_block(victim) != Status::kOk) {
    return FtlStatus::kDeviceError;
  }
  return FtlStatus::kOk;
}

}  // namespace vincentftl
