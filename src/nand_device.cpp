#include "vincentftl/nand_device.hpp"

#include <ostream>

namespace vincentftl {

std::string_view to_string(Status status) {
  switch (status) {
    case Status::kOk:
      return "kOk";
    case Status::kProgramFailed:
      return "kProgramFailed";
    case Status::kEraseFailed:
      return "kEraseFailed";
    case Status::kBadAddress:
      return "kBadAddress";
    case Status::kNotErased:
      return "kNotErased";
    case Status::kOutOfOrder:
      return "kOutOfOrder";
    case Status::kReservedSpareByte:
      return "kReservedSpareByte";
  }
  return "(unknown Status)";
}

std::ostream& operator<<(std::ostream& out, Status status) { return out << to_string(status); }

}  // namespace vincentftl
