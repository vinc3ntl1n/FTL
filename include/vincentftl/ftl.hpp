#pragma once

#include <cstdint>
#include <iosfwd>
#include <string_view>

#include "vincentftl/geometry.hpp"

namespace vincentftl {

// What an Ftl call reports back to the host.
enum class FtlStatus {
  kOk,
  kBadAddress,   // the logical page is not below capacity()
  kNotWritten,   // read of a logical page that has never been written
  kNoSpace,      // no usable block left to write into
  kDeviceError,  // the NandDevice failed in a way the FTL couldn't work around
};

std::string_view to_string(FtlStatus status);
std::ostream& operator<<(std::ostream& out, FtlStatus status);

// What the host sees: pages it can overwrite in place, numbered 0 to capacity() - 1. Behind this,
// the FTL turns every write into a program of a fresh physical page on a NandDevice.
class Ftl {
 public:
  virtual ~Ftl() = default;

  // Number of logical pages the host can use. Smaller than the device, because the FTL keeps
  // spare blocks for garbage collection and bad blocks.
  virtual std::uint32_t capacity() const = 0;

  // Stores `data` at `page`, replacing what was there. A kOk result means the write is
  // acknowledged: it must still be readable after a power cut and a mount().
  virtual FtlStatus write(LogicalPage page, const PageData& data) = 0;

  // Copies the last data written to `page` into `data`.
  virtual FtlStatus read(LogicalPage page, PageData& data) = 0;

  // Rebuilds the mapping table from the spare areas on the device, e.g. after a restart or a
  // power cut. Call it once before the first read or write.
  virtual FtlStatus mount() = 0;
};

}  // namespace vincentftl
