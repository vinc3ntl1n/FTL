#include "vincentftl/block_manager.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <set>

#include "vincentftl/nand_emulator.hpp"

namespace vincentftl {
namespace {

constexpr std::uint32_t kPagesPerBlock = 64;

TEST(BlockManager, HandsOutEveryPageExactlyOnce) {  // test 1
  NandEmulator device(4);
  BlockManager blocks(device);

  std::set<std::uint32_t> seen;

  for (std::uint32_t i = 0; i < 4 * kPagesPerBlock; ++i) {
    const auto page = blocks.allocate_page();
    ASSERT_TRUE(page.has_value());

    // insert().second is false if this page was already in the set.
    EXPECT_TRUE(seen.insert(page->value).second);

    EXPECT_EQ(page->value, i);
    EXPECT_TRUE(blocks.is_valid(*page));
  }
}

TEST(BlockManager, EveryPageItHandsOutCanBeProgrammed) {  // test 2
  NandEmulator device(4);
  BlockManager blocks(device);

  for (std::uint32_t i = 0; i < 4 * kPagesPerBlock; ++i) {
    const auto page = blocks.allocate_page();
    ASSERT_TRUE(page.has_value());

    PageData written;
    written.fill(static_cast<std::uint8_t>(i));

    ASSERT_EQ(device.program_page(*page, written, erased_spare()), Status::kOk);
  }

  EXPECT_FALSE(blocks.allocate_page().has_value());
}

TEST(BlockManager, NeverHandsOutAPageInAFactoryBadBlock) {  // test 3
  NandEmulator device(4, {BlockNumber{2}});
  BlockManager blocks(device);

  EXPECT_EQ(blocks.free_block_count(), 3u);

  for (std::uint32_t b = 0; b < 4; ++b) {
    if (b == 2) {
      continue;
    }

    for (std::uint32_t n = 0; n < kPagesPerBlock; ++n) {
      const auto page = blocks.allocate_page();
      ASSERT_TRUE(page.has_value());

      const auto expected = PhysicalPage::in_block(BlockNumber{b}, n);

      EXPECT_NE(page->block().value, 2u);
      EXPECT_EQ(page->value, expected.value);
      EXPECT_TRUE(blocks.is_valid(*page));
    }
  }

  EXPECT_EQ(blocks.valid_pages(BlockNumber{2}), 0u);
  EXPECT_EQ(blocks.free_block_count(), 0u);
  EXPECT_FALSE(blocks.allocate_page().has_value());
}

TEST(BlockManager, CountsLivePagesPerBlock) {  // test 4
  NandEmulator device(4);                      // a small fake chip: 4 blocks
  BlockManager blocks(device);

  const auto first = blocks.allocate_page();
  const auto second = blocks.allocate_page();
  ASSERT_TRUE(first.has_value() && second.has_value());
  EXPECT_EQ(blocks.valid_pages(first->block()), 2u);

  blocks.mark_invalid(*first);

  EXPECT_EQ(blocks.valid_pages(first->block()), 1u);
  EXPECT_FALSE(blocks.is_valid(*first));
  EXPECT_TRUE(blocks.is_valid(*second));
}

TEST(BlockManager, MarkingAPageInvalidTwiceCountsOnce) {  // test 5
  NandEmulator device(4);
  BlockManager blocks(device);

  const auto page = blocks.allocate_page();
  ASSERT_TRUE(page.has_value());

  EXPECT_EQ(blocks.valid_pages(page->block()), 1u);

  blocks.mark_invalid(*page);

  EXPECT_EQ(blocks.valid_pages(page->block()), 0u);
  EXPECT_FALSE(blocks.is_valid(*page));

  blocks.mark_invalid(*page);

  EXPECT_EQ(blocks.valid_pages(page->block()), 0u);
  EXPECT_FALSE(blocks.is_valid(*page));

  // Invalidating a page must not make its block free again.
  EXPECT_EQ(blocks.free_block_count(), 3u);

  // The invalidated page must not be handed out again.
  const auto next = blocks.allocate_page();
  ASSERT_TRUE(next.has_value());
  EXPECT_NE(next->value, page->value);
}

TEST(BlockManager, APageNeverHandedOutIsNotValid) {  // test 6
  NandEmulator device(4);
  BlockManager blocks(device);

  for (std::uint32_t b = 0; b < 4; ++b) {
    EXPECT_EQ(blocks.valid_pages(BlockNumber{b}), 0u);

    for (std::uint32_t n = 0; n < kPagesPerBlock; ++n) {
      const auto page = PhysicalPage::in_block(BlockNumber{b}, n);
      EXPECT_FALSE(blocks.is_valid(page));
    }
  }

  // invalidating an unallocated page should change nothing.
  const auto unused = PhysicalPage::in_block(BlockNumber{0}, 10);
  blocks.mark_invalid(unused);

  EXPECT_EQ(blocks.valid_pages(BlockNumber{0}), 0u);
  EXPECT_EQ(blocks.free_block_count(), 4u);
}

TEST(BlockManager, FreeBlockCountDropsWhenABlockIsOpened) {  // test 7
  NandEmulator device(4);
  BlockManager blocks(device);

  EXPECT_EQ(blocks.free_block_count(), 4u);

  for (std::uint32_t b = 0; b < 4; ++b) {
    // before opening this block, it is still counted as free.
    EXPECT_EQ(blocks.free_block_count(), 4u - b);

    for (std::uint32_t n = 0; n < kPagesPerBlock; ++n) {
      const auto page = blocks.allocate_page();
      ASSERT_TRUE(page.has_value());

      EXPECT_EQ(page->block().value, b);

      // only the first allocation in each block decreases the count.
      EXPECT_EQ(blocks.free_block_count(), 3u - b);
    }
  }

  EXPECT_EQ(blocks.free_block_count(), 0u);
  EXPECT_FALSE(blocks.allocate_page().has_value());
  EXPECT_EQ(blocks.free_block_count(), 0u);
}

}  // namespace
}  // namespace vincentftl
