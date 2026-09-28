#pragma once

#include <cstdint>
#include <iosfwd>
#include <string_view>

#include "vincentftl/geometry.hpp"

namespace vincentftl {

enum class Status {
  kOk,
  kProgramFailed,
  kEraseFailed,
  kBadAddress,
  kNotErased,
  kOutOfOrder,
  kReservedSpareByte,
};

std::string_view to_string(Status status);
std::ostream& operator<<(std::ostream& out, Status status);

class NandDevice {
 public:
  virtual ~NandDevice() = default;

  virtual std::uint32_t block_count() const = 0;
  virtual Status read_page(PhysicalPage page, PageData& data, PageSpare& spare) = 0;
  virtual Status read_spare(PhysicalPage page, PageSpare& spare) = 0;
  virtual Status program_page(PhysicalPage page, const PageData& data, const PageSpare& spare) = 0;
  virtual Status erase_block(BlockNumber block) = 0;
  virtual bool is_factory_bad(BlockNumber block) = 0;
};

}  // namespace vincentftl
