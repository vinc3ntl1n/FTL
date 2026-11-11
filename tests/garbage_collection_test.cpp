#include "vincentftl/garbage_collection.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "vincentftl/nand_emulator.hpp"

namespace vincentftl {
namespace {

// Every page gets its own contents, so a page copied to the wrong place shows up.
PageData data_for(std::uint32_t tag) {
  PageData data;
  for (std::size_t i = 0; i < kPageDataBytes; ++i) {
    data[i] = static_cast<std::uint8_t>(tag * 7 + i);
  }
  return data;
}

// Fills only the bytes the FTL owns; the chip's reserved bytes stay erased.
PageSpare spare_for(std::uint32_t tag) {
  PageSpare spare = erased_spare();
  for (std::size_t i = 0; i < kPageSpareBytes; ++i) {
    if (!is_reserved_spare_byte(i)) {
      spare[i] = static_cast<std::uint8_t>(tag + i);
    }
  }
  return spare;
}

// Allocates and programs `count` pages, tagging each with its physical page number.
void write_pages(NandDevice& device, BlockManager& blocks, std::uint32_t count) {
  for (std::uint32_t i = 0; i < count; ++i) {
    const auto page = blocks.allocate_page();
    ASSERT_TRUE(page.has_value());
    ASSERT_EQ(device.program_page(*page, data_for(page->value), spare_for(page->value)),
              Status::kOk);
  }
}

// Passes every call through to an emulator, except the one kind of call told to fail.
class FaultyDevice final : public NandDevice {
 public:
  explicit FaultyDevice(std::uint32_t block_count) : chip_(block_count) {}

  std::optional<PhysicalPage> fail_read;
  std::optional<PhysicalPage> fail_program;
  std::optional<BlockNumber> fail_erase;

  NandEmulator& chip() { return chip_; }

  std::uint32_t block_count() const override { return chip_.block_count(); }
  Status read_page(PhysicalPage page, PageData& data, PageSpare& spare) override {
    if (fail_read == page) return Status::kBadAddress;
    return chip_.read_page(page, data, spare);
  }
  Status read_spare(PhysicalPage page, PageSpare& spare) override {
    return chip_.read_spare(page, spare);
  }
  Status program_page(PhysicalPage page, const PageData& data, const PageSpare& spare) override {
    if (fail_program == page) return Status::kProgramFailed;
    return chip_.program_page(page, data, spare);
  }
  Status erase_block(BlockNumber block) override {
    if (fail_erase == block) return Status::kEraseFailed;
    return chip_.erase_block(block);
  }
  bool is_factory_bad(BlockNumber block) override { return chip_.is_factory_bad(block); }

 private:
  NandEmulator chip_;
};

TEST(GarbageCollection, MovesEveryLivePageAndErasesTheVictim) {  // test 1
  NandEmulator device(4);
  BlockManager blocks(device);
  write_pages(device, blocks, kPagesPerBlock);  // block 0, full and all live

  std::vector<PageMove> moves;
  ASSERT_EQ(collect_block(device, blocks, BlockNumber{0}, moves), FtlStatus::kOk);

  ASSERT_EQ(moves.size(), kPagesPerBlock);
  for (std::uint32_t n = 0; n < kPagesPerBlock; ++n) {
    const PageMove& move = moves[n];
    EXPECT_EQ(move.from, PhysicalPage::in_block(BlockNumber{0}, n));
    EXPECT_NE(move.to.block(), BlockNumber{0});

    // The new copy holds the old page's data and the FTL's spare bytes.
    PageData data;
    PageSpare spare;
    ASSERT_EQ(device.read_page(move.to, data, spare), Status::kOk);
    EXPECT_EQ(data, data_for(move.from.value));
    EXPECT_EQ(spare, spare_for(move.from.value));

    EXPECT_FALSE(blocks.is_valid(move.from));
    EXPECT_TRUE(blocks.is_valid(move.to));
  }

  EXPECT_EQ(blocks.valid_pages(BlockNumber{0}), 0u);
  EXPECT_EQ(blocks.valid_pages(BlockNumber{1}), kPagesPerBlock);
  EXPECT_EQ(device.erase_count(BlockNumber{0}), 1u);
}

TEST(GarbageCollection, CopiesOnlyLivePages) {  // test 2
  NandEmulator device(4);
  BlockManager blocks(device);
  write_pages(device, blocks, kPagesPerBlock);

  // Every even page in block 0 has been overwritten somewhere else.
  for (std::uint32_t n = 0; n < kPagesPerBlock; n += 2) {
    blocks.mark_invalid(PhysicalPage::in_block(BlockNumber{0}, n));
  }
  const std::uint64_t programmed_before = device.pages_programmed();

  std::vector<PageMove> moves;
  ASSERT_EQ(collect_block(device, blocks, BlockNumber{0}, moves), FtlStatus::kOk);

  ASSERT_EQ(moves.size(), kPagesPerBlock / 2);
  for (const PageMove& move : moves) {
    EXPECT_EQ(move.from.page_in_block() % 2, 1u);

    PageData data;
    PageSpare spare;
    ASSERT_EQ(device.read_page(move.to, data, spare), Status::kOk);
    EXPECT_EQ(data, data_for(move.from.value));
  }
  EXPECT_EQ(device.pages_programmed() - programmed_before, kPagesPerBlock / 2);
  EXPECT_EQ(blocks.valid_pages(BlockNumber{1}), kPagesPerBlock / 2);
}

TEST(GarbageCollection, AllGarbageBlockIsErasedWithoutCopying) {  // test 3
  NandEmulator device(4);
  BlockManager blocks(device);
  write_pages(device, blocks, kPagesPerBlock);
  for (std::uint32_t n = 0; n < kPagesPerBlock; ++n) {
    blocks.mark_invalid(PhysicalPage::in_block(BlockNumber{0}, n));
  }
  const std::uint64_t programmed_before = device.pages_programmed();

  std::vector<PageMove> moves;
  ASSERT_EQ(collect_block(device, blocks, BlockNumber{0}, moves), FtlStatus::kOk);

  EXPECT_TRUE(moves.empty());
  EXPECT_EQ(device.pages_programmed(), programmed_before);
  EXPECT_EQ(device.erase_count(BlockNumber{0}), 1u);
  // Nothing was allocated, so no block was opened.
  EXPECT_EQ(blocks.free_block_count(), 3u);
}

TEST(GarbageCollection, ErasedVictimCanBeProgrammedFromPageZero) {  // test 4
  NandEmulator device(4);
  BlockManager blocks(device);
  write_pages(device, blocks, kPagesPerBlock);

  std::vector<PageMove> moves;
  ASSERT_EQ(collect_block(device, blocks, BlockNumber{0}, moves), FtlStatus::kOk);

  for (std::uint32_t n = 0; n < kPagesPerBlock; ++n) {
    PageData data;
    PageSpare spare;
    const auto page = PhysicalPage::in_block(BlockNumber{0}, n);
    ASSERT_EQ(device.read_page(page, data, spare), Status::kOk);
    PageData erased;
    erased.fill(kErasedByte);
    EXPECT_EQ(data, erased);
    EXPECT_EQ(spare, erased_spare());
  }
  EXPECT_EQ(
      device.program_page(PhysicalPage::in_block(BlockNumber{0}, 0), data_for(1), erased_spare()),
      Status::kOk);
}

TEST(GarbageCollection, AddsToMovesWithoutClearingIt) {  // test 5
  NandEmulator device(4);
  BlockManager blocks(device);
  write_pages(device, blocks, kPagesPerBlock);
  for (std::uint32_t n = 1; n < kPagesPerBlock; ++n) {
    blocks.mark_invalid(PhysicalPage::in_block(BlockNumber{0}, n));
  }

  const PageMove earlier{PhysicalPage{123}, PhysicalPage{200}};
  std::vector<PageMove> moves{earlier};
  ASSERT_EQ(collect_block(device, blocks, BlockNumber{0}, moves), FtlStatus::kOk);

  ASSERT_EQ(moves.size(), 2u);
  EXPECT_EQ(moves[0].from, earlier.from);
  EXPECT_EQ(moves[0].to, earlier.to);
  EXPECT_EQ(moves[1].from, PhysicalPage::in_block(BlockNumber{0}, 0));
}

TEST(GarbageCollection, NoBlankPagesLeavesTheVictimAlone) {  // test 6
  NandEmulator device(2);
  BlockManager blocks(device);
  write_pages(device, blocks, 2 * kPagesPerBlock);  // the whole chip is full

  std::vector<PageMove> moves;
  EXPECT_EQ(collect_block(device, blocks, BlockNumber{0}, moves), FtlStatus::kNoSpace);

  EXPECT_TRUE(moves.empty());
  EXPECT_EQ(device.erase_count(BlockNumber{0}), 0u);
  EXPECT_EQ(blocks.valid_pages(BlockNumber{0}), kPagesPerBlock);
}

TEST(GarbageCollection, RunningOutHalfwayKeepsUncopiedPagesLive) {  // test 7
  NandEmulator device(2);
  BlockManager blocks(device);
  write_pages(device, blocks, kPagesPerBlock);      // block 0, all live
  write_pages(device, blocks, kPagesPerBlock - 4);  // block 1, 4 blank pages left

  std::vector<PageMove> moves;
  EXPECT_EQ(collect_block(device, blocks, BlockNumber{0}, moves), FtlStatus::kNoSpace);

  // The four pages that fit were copied, and the FTL is told about them.
  ASSERT_EQ(moves.size(), 4u);
  for (const PageMove& move : moves) {
    PageData data;
    PageSpare spare;
    ASSERT_EQ(device.read_page(move.to, data, spare), Status::kOk);
    EXPECT_EQ(data, data_for(move.from.value));
  }
  // The rest still live in the victim, which was not erased.
  EXPECT_EQ(blocks.valid_pages(BlockNumber{0}), kPagesPerBlock - 4);
  EXPECT_EQ(device.erase_count(BlockNumber{0}), 0u);
  PageData data;
  PageSpare spare;
  const auto uncopied = PhysicalPage::in_block(BlockNumber{0}, 4);
  ASSERT_EQ(device.read_page(uncopied, data, spare), Status::kOk);
  EXPECT_EQ(data, data_for(uncopied.value));
}

TEST(GarbageCollection, ReadFailureStopsBeforeCopying) {  // test 8
  FaultyDevice device(4);
  BlockManager blocks(device);
  write_pages(device, blocks, kPagesPerBlock);
  device.fail_read = PhysicalPage::in_block(BlockNumber{0}, 0);

  std::vector<PageMove> moves;
  EXPECT_EQ(collect_block(device, blocks, BlockNumber{0}, moves), FtlStatus::kDeviceError);

  EXPECT_TRUE(moves.empty());
  EXPECT_TRUE(blocks.is_valid(PhysicalPage::in_block(BlockNumber{0}, 0)));
  EXPECT_EQ(device.chip().erase_count(BlockNumber{0}), 0u);
}

TEST(GarbageCollection, ProgramFailureKeepsTheOldCopyLive) {  // test 9
  FaultyDevice device(4);
  BlockManager blocks(device);
  write_pages(device, blocks, kPagesPerBlock);
  // The first page GC copies into is the first page of block 1.
  device.fail_program = PhysicalPage::in_block(BlockNumber{1}, 0);

  std::vector<PageMove> moves;
  EXPECT_EQ(collect_block(device, blocks, BlockNumber{0}, moves), FtlStatus::kDeviceError);

  EXPECT_TRUE(moves.empty());
  EXPECT_TRUE(blocks.is_valid(PhysicalPage::in_block(BlockNumber{0}, 0)));
  // The page that failed to program holds nothing, so it is not counted as live.
  EXPECT_FALSE(blocks.is_valid(PhysicalPage::in_block(BlockNumber{1}, 0)));
  EXPECT_EQ(blocks.valid_pages(BlockNumber{1}), 0u);
  EXPECT_EQ(device.chip().erase_count(BlockNumber{0}), 0u);
}

TEST(GarbageCollection, EraseFailureIsReportedAfterTheCopies) {  // test 10
  FaultyDevice device(4);
  BlockManager blocks(device);
  write_pages(device, blocks, kPagesPerBlock);
  device.fail_erase = BlockNumber{0};

  std::vector<PageMove> moves;
  EXPECT_EQ(collect_block(device, blocks, BlockNumber{0}, moves), FtlStatus::kDeviceError);

  // The data is safe in its new place, so the FTL still needs every move.
  EXPECT_EQ(moves.size(), kPagesPerBlock);
  EXPECT_EQ(blocks.valid_pages(BlockNumber{1}), kPagesPerBlock);
}

TEST(GarbageCollection, NeverCopiesIntoTheVictimItself) {  // test 11
  NandEmulator device(4);
  BlockManager blocks(device);
  // Block 0 is still the open block: 10 pages written, 54 blank.
  write_pages(device, blocks, 10);

  std::vector<PageMove> moves;
  const FtlStatus status = collect_block(device, blocks, BlockNumber{0}, moves);

  // Whatever it returns, no live page may end up in the block that gets erased.
  for (const PageMove& move : moves) {
    EXPECT_NE(move.to.block(), BlockNumber{0})
        << "page " << move.from.value << " was copied into the victim";
  }
  if (status == FtlStatus::kOk) {
    for (const PageMove& move : moves) {
      PageData data;
      PageSpare spare;
      ASSERT_EQ(device.read_page(move.to, data, spare), Status::kOk);
      EXPECT_TRUE(data == data_for(move.from.value)) << "page " << move.from.value << " was lost";
    }
  }
}

}  // namespace
}  // namespace vincentftl
