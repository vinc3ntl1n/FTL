#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <utility>

#include "vincentftl/block_manager.hpp"
#include "vincentftl/differential.hpp"
#include "vincentftl/nand_emulator.hpp"
#include "vincentftl/page_ftl.hpp"
#include "vincentftl/reference_ftl.hpp"

namespace vincentftl {
namespace {

void fill_blocks(BlockManager& blocks, std::uint32_t count) {
  for (std::uint32_t i = 0; i < count * kPagesPerBlock; ++i) {
    ASSERT_TRUE(blocks.allocate_page().has_value()) << "allocation " << i;
  }
}

void erase(NandEmulator& device, BlockManager& blocks, BlockNumber block) {
  for (std::uint32_t n = 0; n < kPagesPerBlock; ++n) {
    blocks.mark_invalid(PhysicalPage::in_block(block, n));
  }
  ASSERT_EQ(device.erase_block(block), Status::kOk);
  blocks.block_erased(block);
}

void make_two_free_blocks(NandEmulator& device, BlockManager& blocks) {
  fill_blocks(blocks, 4);
  erase(device, blocks, BlockNumber{0});
  fill_blocks(blocks, 1);
  erase(device, blocks, BlockNumber{0});
  erase(device, blocks, BlockNumber{1});
}

TEST(WearLeveling, ColdestIsTheFullBlockErasedLeast) {
  NandEmulator device(4);
  BlockManager blocks(device);
  fill_blocks(blocks, 4);
  erase(device, blocks, BlockNumber{0});
  fill_blocks(blocks, 1);
  erase(device, blocks, BlockNumber{0});

  EXPECT_EQ(blocks.pick_coldest(), BlockNumber{1});
  EXPECT_EQ(blocks.max_erase_count(), 2u);
}

TEST(WearLeveling, NoColdestBlockWhenNoneIsFull) {
  NandEmulator device(4);
  BlockManager blocks(device);

  ASSERT_TRUE(blocks.allocate_page().has_value());
  EXPECT_EQ(blocks.pick_coldest(), std::nullopt);
}

TEST(WearLeveling, WithoutItTheLowestNumberedFreeBlockIsOpened) {
  NandEmulator device(4);
  BlockManager blocks(device);
  make_two_free_blocks(device, blocks);

  EXPECT_EQ(blocks.allocate_page(), PhysicalPage::in_block(BlockNumber{0}, 0));
}

TEST(WearLeveling, WithItTheLeastWornFreeBlockIsOpened) {
  NandEmulator device(4);
  BlockManager blocks(device);
  make_two_free_blocks(device, blocks);
  blocks.prefer_least_worn(true);

  EXPECT_EQ(blocks.allocate_page(), PhysicalPage::in_block(BlockNumber{1}, 0));
}

void write_hot_and_cold(PageFtl& ftl, std::uint32_t writes) {
  std::mt19937 rng(1);
  const std::uint32_t hot = ftl.capacity() / 10;
  PageData data;
  for (std::uint32_t page = 0; page < ftl.capacity(); ++page) {
    data.fill(0);
    data[0] = static_cast<std::uint8_t>(page);
    ASSERT_EQ(ftl.write(LogicalPage{page}, data), FtlStatus::kOk);
  }
  for (std::uint32_t i = 0; i < writes; ++i) {
    const std::uint32_t page = rng() % 10 != 0 ? rng() % hot : rng() % ftl.capacity();
    data.fill(static_cast<std::uint8_t>(i));
    data[0] = static_cast<std::uint8_t>(page);
    ASSERT_EQ(ftl.write(LogicalPage{page}, data), FtlStatus::kOk) << "write " << i;
  }
}

std::pair<std::uint32_t, std::uint32_t> least_and_most_erases(NandEmulator& device) {
  std::uint32_t least = UINT32_MAX;
  std::uint32_t most = 0;
  for (std::uint32_t block = 0; block < device.block_count(); ++block) {
    if (device.is_factory_bad(BlockNumber{block})) continue;
    least = std::min(least, device.erase_count(BlockNumber{block}));
    most = std::max(most, device.erase_count(BlockNumber{block}));
  }
  return {least, most};
}

TEST(WearLeveling, WithoutItSomeBlocksAreNeverErased) {
  NandEmulator device(16);
  PageFtl ftl(device, WearLeveling::kOff);
  write_hot_and_cold(ftl, 20'000);

  const auto [least, most] = least_and_most_erases(device);
  EXPECT_EQ(least, 0u);
  EXPECT_GT(most, 80u);
}

TEST(WearLeveling, KeepsEveryBlockWithinTheGap) {
  NandEmulator device(16, {BlockNumber{3}});
  PageFtl ftl(device, WearLeveling::kOn);
  write_hot_and_cold(ftl, 20'000);

  const auto [least, most] = least_and_most_erases(device);
  EXPECT_LE(most - least, kWearLevelingGap);
}

TEST(WearLeveling, MovedDataStillReadsBack) {
  NandEmulator device(16, {BlockNumber{3}});
  PageFtl ftl(device, WearLeveling::kOn);
  write_hot_and_cold(ftl, 20'000);

  PageData data;
  for (std::uint32_t page = 0; page < ftl.capacity(); ++page) {
    ASSERT_EQ(ftl.read(LogicalPage{page}, data), FtlStatus::kOk);
    EXPECT_EQ(data[0], static_cast<std::uint8_t>(page));
  }
}

TEST(WearLeveling, PageFtlWithItAgreesWithReferenceFor100000Operations) {
  NandEmulator device(32);
  PageFtl subject(device, WearLeveling::kOn);
  ReferenceFtl reference(subject.capacity());

  const auto mismatch = run_differential(subject, reference, 1, 100'000);

  EXPECT_FALSE(mismatch.has_value()) << mismatch.value_or("");
}

}  // namespace
}  // namespace vincentftl
