#include "vincentftl/page_ftl.hpp"

namespace vincentftl {

PageFtl::PageFtl(NandDevice& device) : device_(device) {

  for (std::uint32_t i = 0; i < device_.block_count(); ++i) {
    if (!device_.is_factory_bad(BlockNumber{i})) {
      capacity_ += kPagesPerBlock;
    }
  }
  map_.resize(capacity_, std::nullopt);
  
}

std::uint32_t PageFtl::capacity() const {
  return capacity_;
}

FtlStatus PageFtl::write(LogicalPage page, const PageData& data) {
  if (page.value >= capacity_) {
    return FtlStatus::kBadAddress;
  }

  const std::uint32_t total = device_.block_count() * kPagesPerBlock;

  while (next_physical_ < total && device_.is_factory_bad(PhysicalPage{next_physical_}.block())) {
    ++next_physical_;
  }
  if (next_physical_ >= total) {
    return FtlStatus::kNoSpace;
  }

  const PhysicalPage target{next_physical_};
  if (device_.program_page(target, data, erased_spare()) != Status::kOk) {
    return FtlStatus::kDeviceError;
  }

  ++next_physical_;
  map_[page.value] = target;
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

FtlStatus PageFtl::mount() {
  return FtlStatus::kOk;
}

}  // namespace vincentftl
