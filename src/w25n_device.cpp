// The parts of W25nDevice that both pairs use. The rest is in w25n_control.cpp and w25n_pages.cpp.

#include "vincentftl/w25n_device.hpp"

#include <chrono>
#include <stdexcept>

namespace vincentftl {

W25nDevice::W25nDevice(SpiBus& bus, std::uint32_t block_count,
                       const std::vector<BlockNumber>& factory_bad_blocks)
    : bus_(bus), block_count_(block_count), factory_bad_(block_count, false) {
  if (block_count == 0 || block_count > kBlockCount) {
    throw std::invalid_argument("W25nDevice: block_count must be from 1 to 1024");
  }
  for (const BlockNumber block : factory_bad_blocks) {
    if (block.value < block_count_) {
      factory_bad_[block.value] = true;
    }
  }
}

std::uint32_t W25nDevice::block_count() const { return block_count_; }

bool W25nDevice::is_factory_bad(BlockNumber block) {
  return block.value < block_count_ && factory_bad_[block.value];
}

std::uint8_t W25nDevice::read_register(std::uint8_t address) {
  // Opcode, register address, then one byte comes back (8.2.3).
  return bus_.transfer({0x0F, address, 0x00})[2];
}

void W25nDevice::write_register(std::uint8_t address, std::uint8_t value) {
  bus_.transfer({0x1F, address, value});  // 8.2.4
}

void W25nDevice::write_enable() {
  bus_.transfer({0x06});  // 8.2.5
}

std::optional<std::uint8_t> W25nDevice::wait_ready() {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
  while (true) {
    const std::uint8_t status = read_register(kStatusRegister);
    if ((status & kBusy) == 0) {
      return status;
    }
    if (std::chrono::steady_clock::now() > deadline) {
      return std::nullopt;
    }
  }
}

}  // namespace vincentftl
