#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>

namespace vincentftl {

inline constexpr std::size_t kPageDataBytes = 2048;
inline constexpr std::size_t kPageSpareBytes = 64;
inline constexpr std::uint32_t kPagesPerBlock = 64;
inline constexpr std::uint32_t kBlockCount = 1024;
inline constexpr std::uint32_t kPageCount = kBlockCount * kPagesPerBlock;

inline constexpr std::uint8_t kErasedByte = 0xFF;

using PageData = std::array<std::uint8_t, kPageDataBytes>;
using PageSpare = std::array<std::uint8_t, kPageSpareBytes>;

inline constexpr std::size_t kSpareGroupBytes = 16;
constexpr bool is_reserved_spare_byte(std::size_t index) {
  const std::size_t in_group = index % kSpareGroupBytes;
  return in_group < 2 || in_group >= 8;
}

constexpr PageSpare erased_spare() {
  PageSpare spare{};
  spare.fill(kErasedByte);
  return spare;
}

struct BlockNumber {
  std::uint32_t value;
  friend constexpr auto operator<=>(const BlockNumber&, const BlockNumber&) = default;
};

struct LogicalPage {
  std::uint32_t value;
  friend constexpr auto operator<=>(const LogicalPage&, const LogicalPage&) = default;
};

struct PhysicalPage {
  std::uint32_t value;

  static constexpr PhysicalPage in_block(BlockNumber block, std::uint32_t page) {
    return {block.value * kPagesPerBlock + page};
  }
  constexpr BlockNumber block() const { return {value / kPagesPerBlock}; }
  constexpr std::uint32_t page_in_block() const { return value % kPagesPerBlock; }

  friend constexpr auto operator<=>(const PhysicalPage&, const PhysicalPage&) = default;
};

}  // namespace vincentftl
