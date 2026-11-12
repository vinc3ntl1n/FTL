#include "vincentftl/page_ftl.hpp"

#include <vector>

#include "vincentftl/garbage_collection.hpp"

namespace vincentftl {

PageFtl::PageFtl(NandDevice& device, WearLeveling wear_leveling)
    : device_(device), blocks_(device), wear_leveling_(wear_leveling) {
  std::uint32_t good_blocks = 0;
  for (std::uint32_t i = 0; i < device_.block_count(); ++i) {
    if (!device_.is_factory_bad(BlockNumber{i})) {
      ++good_blocks;
    }
  }

  if (good_blocks > kReserveBlocks) {
    capacity_ = (good_blocks - kReserveBlocks) * kPagesPerBlock;
  }

  map_.resize(capacity_, std::nullopt);
  owner_.resize(device_.block_count() * kPagesPerBlock);
  blocks_.prefer_least_worn(wear_leveling_ == WearLeveling::kOn);
}

std::uint32_t PageFtl::capacity() const { return capacity_; }

FtlStatus PageFtl::write(LogicalPage page, const PageData& data) {
  if (page.value >= capacity_) {
    return FtlStatus::kBadAddress;
  }

  const FtlStatus room_status = make_room();
  if (room_status != FtlStatus::kOk) {
    return room_status;
  }
  if (wear_leveling_ == WearLeveling::kOn) {
    const FtlStatus wear_status = level_wear();
    if (wear_status != FtlStatus::kOk) return wear_status;
  }

  const auto target = blocks_.allocate_page();
  if (!target) {
    return FtlStatus::kNoSpace;
  }

  if (device_.program_page(*target, data, erased_spare()) != Status::kOk) {
    blocks_.mark_invalid(*target);
    return FtlStatus::kDeviceError;
  }

  if (map_[page.value]) {
    blocks_.mark_invalid(*map_[page.value]);
  }
  map_[page.value] = *target;
  owner_[target->value] = page;
  return FtlStatus::kOk;
}

FtlStatus PageFtl::make_room() {
  while (blocks_.free_block_count() < 2) {
    const auto victim = blocks_.pick_victim();
    if (!victim) {
      return FtlStatus::kNoSpace;
    }

    const FtlStatus status = clean(*victim);
    if (status != FtlStatus::kOk) return status;
  }

  return FtlStatus::kOk;
}

FtlStatus PageFtl::clean(BlockNumber block) {
  std::vector<PageMove> moves;
  const FtlStatus collect_status = collect_block(device_, blocks_, block, moves);
  for (const PageMove& move : moves) {
    const LogicalPage logical_page = owner_[move.from.value];
    map_[logical_page.value] = move.to;
    owner_[move.to.value] = logical_page;
  }
  if (collect_status != FtlStatus::kOk) return collect_status;
  blocks_.block_erased(block);
  return FtlStatus::kOk;
}

FtlStatus PageFtl::level_wear() {
  const auto coldest = blocks_.pick_coldest();
  if (!coldest) return FtlStatus::kOk;
  const std::uint32_t coldest_count = blocks_.erase_count(*coldest);
  if (blocks_.max_erase_count() - coldest_count < kWearLevelingGap) {
    return FtlStatus::kOk;
  }
  return clean(*coldest);
}

FtlStatus PageFtl::read(LogicalPage page, PageData& data) {
  if (page.value >= capacity_) {
    return FtlStatus::kBadAddress;
  }

  if (!map_[page.value].has_value()) {
    return FtlStatus::kNotWritten;
  }

  PageSpare spare;
  if (device_.read_page(*map_[page.value], data, spare) != Status::kOk) {
    return FtlStatus::kDeviceError;
  }
  return FtlStatus::kOk;
}

FtlStatus PageFtl::mount() { return FtlStatus::kOk; }

}  // namespace vincentftl
