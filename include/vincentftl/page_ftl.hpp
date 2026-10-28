#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "vincentftl/ftl.hpp"
#include "vincentftl/geometry.hpp"
#include "vincentftl/nand_device.hpp"

namespace vincentftl {

class PageFtl final : public Ftl {
 public:
  explicit PageFtl(NandDevice& device);

  std::uint32_t capacity() const override;
  FtlStatus write(LogicalPage page, const PageData& data) override;
  FtlStatus read(LogicalPage page, PageData& data) override;
  FtlStatus mount() override;

 private:
  NandDevice& device_;
  std::uint32_t capacity_ = 0;
  // The mapping table: one entry per logical page. Empty means "never written".
  std::vector<std::optional<PhysicalPage>> map_;
  // The next physical page to try.
  std::uint32_t next_physical_ = 0;
};

}  // namespace vincentftl
