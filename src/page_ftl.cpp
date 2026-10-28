#include "vincentftl/page_ftl.hpp"

namespace vincentftl {

PageFtl::PageFtl(NandDevice& device) : device_(device), blocks_(device) {
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
}

std::uint32_t PageFtl::capacity() const { return capacity_; }

FtlStatus PageFtl::write(LogicalPage page, const PageData& data) {
  if (page.value >= capacity_) {
    return FtlStatus::kBadAddress;
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
