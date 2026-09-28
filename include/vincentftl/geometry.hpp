#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>

namespace vincentftl {

// Sizes of the Winbond W25N01GV. See docs/nand-rules.md.
inline constexpr std::size_t kPageDataBytes = 2048;
inline constexpr std::size_t kPageSpareBytes = 64;
inline constexpr std::uint32_t kPagesPerBlock = 64;
inline constexpr std::uint32_t kBlockCount = 1024;
inline constexpr std::uint32_t kPageCount = kBlockCount * kPagesPerBlock;  // 65,536

// Every byte of an erased page reads back as this.
inline constexpr std::uint8_t kErasedByte = 0xFF;

// The contents of one page. Fixed-size arrays, so a buffer of the wrong size doesn't compile.
using PageData = std::array<std::uint8_t, kPageDataBytes>;
using PageSpare = std::array<std::uint8_t, kPageSpareBytes>;

// With ECC on, the spare area is four 16-byte groups. In each group the FTL owns bytes 2-7 and
// the chip owns the rest (bad-block marker and ECC check bits). See the spare area map in
// docs/nand-rules.md.
inline constexpr std::size_t kSpareGroupBytes = 16;
constexpr bool is_reserved_spare_byte(std::size_t index) {
  const std::size_t in_group = index % kSpareGroupBytes;
  return in_group < 2 || in_group >= 8;
}

// A spare area with every byte 0xFF. Start from this when building one to program, since the
// chip-owned bytes must stay 0xFF. (PageSpare{} would be all zeros, and the program would fail.)
constexpr PageSpare erased_spare() {
  PageSpare spare{};
  spare.fill(kErasedByte);
  return spare;
}

// Three kinds of number that are all "just an integer" underneath. Wrapping each in its own type
// means the compiler rejects code that passes one where another is expected, e.g. handing a
// logical page to the chip. Write them as LogicalPage{7}, and read the number back with .value.

// A block on the chip, counted from 0.
struct BlockNumber {
  std::uint32_t value;
  friend constexpr auto operator<=>(const BlockNumber&, const BlockNumber&) = default;
};

// Where the host thinks its data lives: 0 up to the FTL's capacity.
struct LogicalPage {
  std::uint32_t value;
  friend constexpr auto operator<=>(const LogicalPage&, const LogicalPage&) = default;
};

// Where data actually lives on the chip, counted from 0 across the whole chip: pages 0-63 are
// block 0, pages 64-127 are block 1, and so on.
struct PhysicalPage {
  std::uint32_t value;

  // Page `page` (0-63) of block `block`.
  static constexpr PhysicalPage in_block(BlockNumber block, std::uint32_t page) {
    return {block.value * kPagesPerBlock + page};
  }
  constexpr BlockNumber block() const { return {value / kPagesPerBlock}; }
  constexpr std::uint32_t page_in_block() const { return value % kPagesPerBlock; }

  friend constexpr auto operator<=>(const PhysicalPage&, const PhysicalPage&) = default;
};

}  // namespace vincentftl
