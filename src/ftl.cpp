#include "vincentftl/ftl.hpp"

#include <ostream>

namespace vincentftl {

std::string_view to_string(FtlStatus status) {
  // No default case, so the compiler warns if a new FtlStatus is added without a name here.
  switch (status) {
    case FtlStatus::kOk:
      return "kOk";
    case FtlStatus::kBadAddress:
      return "kBadAddress";
    case FtlStatus::kNotWritten:
      return "kNotWritten";
    case FtlStatus::kNoSpace:
      return "kNoSpace";
    case FtlStatus::kDeviceError:
      return "kDeviceError";
  }
  return "(unknown FtlStatus)";
}

std::ostream& operator<<(std::ostream& out, FtlStatus status) { return out << to_string(status); }

}  // namespace vincentftl
