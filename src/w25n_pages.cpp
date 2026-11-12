// Pages pair: replace the body of each function. The Assignment 3 handout says what each one does.
// Until then, they all fail.

#include "vincentftl/w25n_device.hpp"

namespace vincentftl {

Status W25nDevice::read_page(PhysicalPage /*page*/, PageData& /*data*/, PageSpare& /*spare*/) {
  return Status::kBadAddress;
}

Status W25nDevice::read_spare(PhysicalPage /*page*/, PageSpare& /*spare*/) {
  return Status::kBadAddress;
}

Status W25nDevice::program_page(PhysicalPage /*page*/, const PageData& /*data*/,
                                const PageSpare& /*spare*/) {
  return Status::kProgramFailed;
}

}  // namespace vincentftl
