#pragma once

#include <cstdint>
#include <iosfwd>
#include <string_view>

#include "vincentftl/geometry.hpp"

namespace vincentftl {

enum class FtlStatus {
  kOk,
  kBadAddress,
  kNotWritten,
  kNoSpace,
  kDeviceError,
};

std::string_view to_string(FtlStatus status);
std::ostream& operator<<(std::ostream& out, FtlStatus status);

class Ftl {
 public:
  virtual ~Ftl() = default;

  virtual std::uint32_t capacity() const = 0;
  virtual FtlStatus write(LogicalPage page, const PageData& data) = 0;
  virtual FtlStatus read(LogicalPage page, PageData& data) = 0;
  virtual FtlStatus mount() = 0;
};

}  // namespace vincentftl
