#pragma once

#include <cstdint>
#include <iosfwd>
#include <string_view>

#include "vincentftl/geometry.hpp"

namespace vincentftl {

// What a NandDevice call reports back. See docs/nand-rules.md for each rule.
enum class Status {
  kOk,

  // The chip says the operation failed. On real hardware this means a worn-out or bad block, and
  // the FTL should stop using it.
  kProgramFailed,
  kEraseFailed,

  // The page or block number is past the end of the chip.
  kBadAddress,

  // The caller broke one of the chip's rules. These always mean a bug in the caller, not a
  // worn-out block.
  kNotErased,          // the page was already programmed since its block was last erased
  kOutOfOrder,         // a higher page in the same block was already programmed
  kReservedSpareByte,  // the spare area had something other than 0xFF in a byte the chip owns
};

// "kOk", "kNotErased", ... Lets tests print a readable name when a status check fails.
std::string_view to_string(Status status);
std::ostream& operator<<(std::ostream& out, Status status);

// A raw NAND chip. The emulator and the hardware driver both implement this, so the FTL runs on
// either without knowing which one it has.
class NandDevice {
 public:
  virtual ~NandDevice() = default;

  // Number of blocks on this device. The real chip has kBlockCount; the emulator can be made
  // smaller so tests fill it quickly.
  virtual std::uint32_t block_count() const = 0;

  // Copies a page's data and spare area into `data` and `spare`. An erased page reads as all 0xFF.
  virtual Status read_page(PhysicalPage page, PageData& data, PageSpare& spare) = 0;

  // Reads only the spare area. Much faster than read_page on the real chip, since only 64 bytes
  // cross the wire instead of 2112. Scanning every page's spare area is how the FTL mounts.
  virtual Status read_spare(PhysicalPage page, PageSpare& spare) = 0;

  // Writes data and spare area into an erased page.
  virtual Status program_page(PhysicalPage page, const PageData& data, const PageSpare& spare) = 0;

  // Sets every byte of every page in the block back to 0xFF.
  virtual Status erase_block(BlockNumber block) = 0;

  // True if the block was marked bad at the factory. Also true for a block past the end of the
  // device, since that block isn't usable either.
  virtual bool is_factory_bad(BlockNumber block) = 0;
};

}  // namespace vincentftl
