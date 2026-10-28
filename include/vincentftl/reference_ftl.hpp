#pragma once

#include <cstdint>
#include <unordered_map>

#include "vincentftl/ftl.hpp"
#include "vincentftl/geometry.hpp"

namespace vincentftl {

class ReferenceFtl final : public Ftl {
 public:
  explicit ReferenceFtl(std::uint32_t capacity);

  std::uint32_t capacity() const override;
  FtlStatus write(LogicalPage page, const PageData& data) override;
  FtlStatus read(LogicalPage page, PageData& data) override;
  FtlStatus mount() override;

 private:
  std::uint32_t capacity_;
  // Logical page number to its data.
  std::unordered_map<std::uint32_t, PageData> pages_;
};

}  // namespace vincentftl
