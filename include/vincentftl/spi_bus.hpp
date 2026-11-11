#pragma once

#include <cstdint>
#include <vector>

namespace vincentftl {

// The SPI wires between the Pi and the chip. One call to transfer is one SPI transaction: chip
// select goes low, the bytes in `tx` go out one at a time while the same number of bytes come
// back, then chip select goes high again. Every chip command is exactly one transfer.
//
// On the Pi this is LinuxSpiBus. Tests use FakeW25n (tests/fake_w25n.hpp), a pretend chip.
class SpiBus {
 public:
  virtual ~SpiBus() = default;

  // Sends `tx` and returns the bytes that came back while it was sent. The result is always the
  // same length as `tx`.
  virtual std::vector<std::uint8_t> transfer(const std::vector<std::uint8_t>& tx) = 0;
};

}  // namespace vincentftl
