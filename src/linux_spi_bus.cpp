#include "vincentftl/linux_spi_bus.hpp"

#include <stdexcept>

#if defined(__linux__)
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#endif

namespace vincentftl {

#if defined(__linux__)

namespace {

[[noreturn]] void fail(const std::string& what, int error) {
  throw std::runtime_error("LinuxSpiBus: " + what + ": " + std::strerror(error));
}

}  // namespace

LinuxSpiBus::LinuxSpiBus(const std::string& path, std::uint32_t speed_hz) : speed_hz_(speed_hz) {
  fd_ = ::open(path.c_str(), O_RDWR);
  if (fd_ < 0) {
    fail("can't open " + path + " (is SPI turned on? see docs/hardware-setup.md)", errno);
  }
  std::uint8_t mode = SPI_MODE_0;
  std::uint8_t bits_per_word = 8;
  if (::ioctl(fd_, SPI_IOC_WR_MODE, &mode) < 0 ||
      ::ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &bits_per_word) < 0 ||
      ::ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz_) < 0) {
    const int error = errno;
    ::close(fd_);
    fail("can't set up " + path, error);
  }
}

LinuxSpiBus::~LinuxSpiBus() { ::close(fd_); }

std::vector<std::uint8_t> LinuxSpiBus::transfer(const std::vector<std::uint8_t>& tx) {
  std::vector<std::uint8_t> rx(tx.size());
  if (tx.empty()) {
    return rx;
  }
  spi_ioc_transfer message{};
  message.tx_buf = reinterpret_cast<std::uintptr_t>(tx.data());
  message.rx_buf = reinterpret_cast<std::uintptr_t>(rx.data());
  message.len = static_cast<std::uint32_t>(tx.size());
  message.speed_hz = speed_hz_;
  message.bits_per_word = 8;
  if (::ioctl(fd_, SPI_IOC_MESSAGE(1), &message) < 0) {
    fail("transfer failed", errno);
  }
  return rx;
}

#else

LinuxSpiBus::LinuxSpiBus(const std::string& path, std::uint32_t speed_hz) : speed_hz_(speed_hz) {
  throw std::runtime_error("LinuxSpiBus: " + path + " only exists on the Pi. Run this there.");
}

LinuxSpiBus::~LinuxSpiBus() = default;

std::vector<std::uint8_t> LinuxSpiBus::transfer(const std::vector<std::uint8_t>&) {
  throw std::logic_error("LinuxSpiBus: only works on Linux");
}

#endif

}  // namespace vincentftl
