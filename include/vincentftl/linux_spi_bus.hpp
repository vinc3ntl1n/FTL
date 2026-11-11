#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "vincentftl/spi_bus.hpp"

namespace vincentftl {

// The Raspberry Pi's SPI0 bus, through Linux's spidev driver. Needs SPI turned on (see
// docs/hardware-setup.md). Only works on Linux: anywhere else the constructor throws.
class LinuxSpiBus final : public SpiBus {
 public:
  // Opens the bus in SPI mode 0. Throws std::runtime_error if it can't.
  explicit LinuxSpiBus(const std::string& path = "/dev/spidev0.0",
                       std::uint32_t speed_hz = 1'000'000);
  ~LinuxSpiBus() override;

  LinuxSpiBus(const LinuxSpiBus&) = delete;
  LinuxSpiBus& operator=(const LinuxSpiBus&) = delete;

  std::vector<std::uint8_t> transfer(const std::vector<std::uint8_t>& tx) override;

 private:
  [[maybe_unused]] int fd_ = -1;
  [[maybe_unused]] std::uint32_t speed_hz_;
};

}  // namespace vincentftl
