#pragma once

#include <array>
#include <cstdint>
#include <istream>
#include <optional>
#include <vector>

#include "vincentftl/geometry.hpp"
#include "vincentftl/nand_device.hpp"
#include "vincentftl/spi_bus.hpp"

namespace vincentftl {

// The real chip: a NandDevice that sends W25N01GV commands over SPI. The FTL can't tell it from
// the emulator. Section numbers refer to the datasheet, Revision O.
class W25nDevice final : public NandDevice {
 public:
  // The addresses of the chip's three status registers (7.1, 7.2, 7.3).
  static constexpr std::uint8_t kProtectionRegister = 0xA0;
  static constexpr std::uint8_t kConfigRegister = 0xB0;
  static constexpr std::uint8_t kStatusRegister = 0xC0;

  // Bits in kStatusRegister (7.3).
  static constexpr std::uint8_t kBusy = 0x01;
  static constexpr std::uint8_t kWriteEnabled = 0x02;
  static constexpr std::uint8_t kEraseFailedBit = 0x04;
  static constexpr std::uint8_t kProgramFailedBit = 0x08;

  // Uses only the first `block_count` blocks of the chip, so a test or demo fills it quickly.
  // `factory_bad_blocks` is the list tools/pi/scan_bad_blocks.py saved for this chip. Entries past
  // `block_count` are ignored.
  W25nDevice(SpiBus& bus, std::uint32_t block_count,
             const std::vector<BlockNumber>& factory_bad_blocks);

  // ---- Control pair: src/w25n_control.cpp ----

  // Gets the chip ready: resets it, checks its ID, and turns write protection off. Call this once,
  // before anything else. Returns false if the chip doesn't answer with the W25N01GV's ID.
  bool start();

  // The chip's three ID bytes. A W25N01GV answers EF AA 21.
  std::array<std::uint8_t, 3> read_id();

  Status erase_block(BlockNumber block) override;

  // ---- Pages pair: src/w25n_pages.cpp ----

  Status read_page(PhysicalPage page, PageData& data, PageSpare& spare) override;
  Status read_spare(PhysicalPage page, PageSpare& spare) override;
  Status program_page(PhysicalPage page, const PageData& data, const PageSpare& spare) override;

  // ---- Given: src/w25n_device.cpp ----

  std::uint32_t block_count() const override;
  bool is_factory_bad(BlockNumber block) override;

  // Read Status Register (0Fh): the value of one of the three registers above.
  std::uint8_t read_register(std::uint8_t address);

  // Write Status Register (1Fh).
  void write_register(std::uint8_t address, std::uint8_t value);

  // Write Enable (06h). The chip needs it before every program and every erase.
  void write_enable();

  // Waits until the chip has finished what it's doing, then returns kStatusRegister's value. Check
  // its kProgramFailedBit or kEraseFailedBit to see whether a program or erase worked. Returns
  // nothing if the chip is still busy after 50 ms. An erase takes at most 10 ms (9.6), so that
  // usually means a wiring problem.
  std::optional<std::uint8_t> wait_ready();

 private:
  SpiBus& bus_;
  std::uint32_t block_count_;
  std::vector<bool> factory_bad_;
};

// Reads the list tools/pi/scan_bad_blocks.py saves: one block number per line. Lines that are
// empty or start with # are skipped. (Control pair)
std::vector<BlockNumber> read_bad_block_list(std::istream& in);

}  // namespace vincentftl
