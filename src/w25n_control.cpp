// Control pair: chip startup, identification, block erasure, and the bad-block list.
// See Assignment 3 for the required behavior.

#include <chrono>
#include <string>
#include <thread>

#include "vincentftl/w25n_device.hpp"

namespace vincentftl {

bool W25nDevice::start() {
  // Resetting a busy chip can corrupt an erase in progress.
  if (!wait_ready()) {
    return false;
  }

  bus_.transfer({0xFF});  // Device Reset
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  if (!wait_ready()) {
    return false;
  }

  const std::array<std::uint8_t, 3> expected_id = {0xEF, 0xAA, 0x21};
  if (read_id() != expected_id) {
    return false;
  }

  write_register(kProtectionRegister, 0x00);
  write_register(kConfigRegister, 0x18);  // ECC on, buffer read mode

  const auto protection = read_register(kProtectionRegister);
  const auto config = read_register(kConfigRegister);
  return protection == 0x00 && config == 0x18;
}

std::array<std::uint8_t, 3> W25nDevice::read_id() {
  const auto reply = bus_.transfer({0x9F, 0x00, 0x00, 0x00, 0x00});
  return {reply[2], reply[3], reply[4]};
}

Status W25nDevice::erase_block(BlockNumber block) {
  if (block.value >= block_count_) {
    return Status::kBadAddress;
  }
  if (is_factory_bad(block)) {
    return Status::kEraseFailed;
  }

  write_enable();
  const auto page = PhysicalPage::in_block(block, 0).value;
  bus_.transfer(
      {0xD8, 0x00, static_cast<std::uint8_t>(page >> 8), static_cast<std::uint8_t>(page & 0xFF)});

  const auto status = wait_ready();
  if (!status) {
    return Status::kTimeout;
  }
  if ((*status & kEraseFailedBit) != 0) {
    return Status::kEraseFailed;
  }
  return Status::kOk;
}

std::vector<BlockNumber> read_bad_block_list(std::istream& in) {
  std::vector<BlockNumber> blocks;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    blocks.push_back(BlockNumber{static_cast<std::uint32_t>(std::stoul(line))});
  }
  return blocks;
}

}  // namespace vincentftl
