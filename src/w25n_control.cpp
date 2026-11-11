// Control pair: replace the body of each function. The Assignment 3 handout says what each one
// does. Until then, they all fail.

#include "vincentftl/w25n_device.hpp"

namespace vincentftl {

bool W25nDevice::start() { return false; }

std::array<std::uint8_t, 3> W25nDevice::read_id() { return {0, 0, 0}; }

Status W25nDevice::erase_block(BlockNumber /*block*/) { return Status::kEraseFailed; }

std::vector<BlockNumber> read_bad_block_list(std::istream& /*in*/) { return {}; }

}  // namespace vincentftl
