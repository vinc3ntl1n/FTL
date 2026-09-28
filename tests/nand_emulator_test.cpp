#include "vincentftl/nand_emulator.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace vincentftl {
namespace {

// A worked example to copy: set up a device, do one thing, check the result.
TEST(NandEmulator, RejectsProgrammingBelowAnAlreadyProgrammedPage) {
  // Setup. A 4-block device is plenty here and keeps the test fast.
  NandEmulator device(4);
  PageData data;
  data.fill(0xAB);
  const PageSpare spare = erased_spare();  // not PageSpare{}: that's all zeros, see geometry.hpp

  // Program page 5 of block 1. Skipping pages 0-4 is allowed.
  ASSERT_EQ(device.program_page(PhysicalPage::in_block(BlockNumber{1}, 5), data, spare),
            Status::kOk);

  // Going back to page 2 of the same block breaks rule 3 in docs/nand-rules.md.
  EXPECT_EQ(device.program_page(PhysicalPage::in_block(BlockNumber{1}, 2), data, spare),
            Status::kOutOfOrder);
}

// Use ASSERT_* when a failure makes the rest of the test pointless (it stops the test there), and
// EXPECT_* for the checks the test is about (it keeps going and reports every failure).
TEST(NandEmulator, RejectsDataInSpareBytesTheChipOwns) {
  NandEmulator device(4);
  PageData data;
  data.fill(0x00);

  // Byte 4 belongs to the FTL, so this program works.
  PageSpare ftl_metadata = erased_spare();
  ftl_metadata[4] = 0x12;
  ASSERT_EQ(device.program_page(PhysicalPage{0}, data, ftl_metadata), Status::kOk);

  // Byte 8 holds the chip's ECC check bits, so this one is refused.
  PageSpare clobbers_ecc = erased_spare();
  clobbers_ecc[8] = 0x12;
  EXPECT_EQ(device.program_page(PhysicalPage{1}, data, clobbers_ecc), Status::kReservedSpareByte);
}

TEST(NandEmulator, CountsSuccessfulProgramsAndErases) {
  NandEmulator device(4);
  PageData data;
  data.fill(0x5A);
  const PageSpare spare = erased_spare();

  ASSERT_EQ(device.program_page(PhysicalPage{0}, data, spare), Status::kOk);
  ASSERT_EQ(device.program_page(PhysicalPage{1}, data, spare), Status::kOk);
  ASSERT_EQ(device.program_page(PhysicalPage{1}, data, spare), Status::kNotErased);  // not counted
  ASSERT_EQ(device.erase_block(BlockNumber{0}), Status::kOk);
  ASSERT_EQ(device.erase_block(BlockNumber{2}), Status::kOk);

  EXPECT_EQ(device.pages_programmed(), 2u);
  EXPECT_EQ(device.blocks_erased(), 2u);
}

TEST(NandEmulator, FactoryBadBlockShowsTheFactoryMarker) {
  NandEmulator device(4, {BlockNumber{2}});
  PageData data;
  PageSpare spare;

  ASSERT_EQ(device.read_page(PhysicalPage::in_block(BlockNumber{2}, 0), data, spare), Status::kOk);

  EXPECT_NE(data[0], kErasedByte);
  EXPECT_NE(spare[0], kErasedByte);
}

TEST(NandEmulator, RefusesADeviceTheChipCouldNotBe) {
  EXPECT_THROW(NandEmulator(0), std::invalid_argument);
  EXPECT_THROW(NandEmulator(kBlockCount + 1), std::invalid_argument);
  EXPECT_THROW(NandEmulator(4, {BlockNumber{4}}), std::invalid_argument);  // past the end
  EXPECT_THROW(NandEmulator(4, {BlockNumber{0}}), std::invalid_argument);  // always good
}

}  // namespace
}  // namespace vincentftl
