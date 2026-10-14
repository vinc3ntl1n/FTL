#include "vincentftl/reference_ftl.hpp"

namespace vincentftl {

    ReferenceFtl::ReferenceFtl(std::uint32_t capacity) : capacity_(capacity) {}

    std::uint32_t ReferenceFtl::capacity() const {
        return capacity_;
    }

    FtlStatus ReferenceFtl::write(LogicalPage page, const PageData& data) {
        if (page.value >= capacity_) {
            return FtlStatus::kBadAddress;
        }
        pages_[page.value] = data;
        return FtlStatus::kOk;
    }

    FtlStatus ReferenceFtl::read(LogicalPage page, PageData& data) {
        if (page.value >= capacity_) {
            return FtlStatus::kBadAddress;
        }
        auto it = pages_.find(page.value);
        if (it == pages_.end()) {
            return FtlStatus::kNotWritten;
        }
        data = it->second;
        return FtlStatus::kOk;
    }

    FtlStatus ReferenceFtl::mount() {
        return FtlStatus::kOk;
    }

} // namespace vincentftl