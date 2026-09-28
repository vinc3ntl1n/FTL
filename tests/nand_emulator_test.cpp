#include "vincentftl/nand_emulator.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace vincentftl {
namespace {

TEST(NandEmulator, RejectsProgrammingBelowAnAlreadyProgrammedPage) {
  NandEmulator device(4);
  PageData data;
  data.fill(0xAB);
  const PageSpare spare = erased_spare();

  ASSERT_EQ(device.program_page(PhysicalPage::in_block(BlockNumber{1}, 5), data, spare),
            Status::kOk);

  EXPECT_EQ(device.program_page(PhysicalPage::in_block(BlockNumber{1}, 2), data, spare),
            Status::kOutOfOrder);
}

TEST(NandEmulator, RejectsDataInSpareBytesTheChipOwns) {
  NandEmulator device(4);
  PageData data;
  data.fill(0x00);

  PageSpare ftl_metadata = erased_spare();
  ftl_metadata[4] = 0x12;
  ASSERT_EQ(device.program_page(PhysicalPage{0}, data, ftl_metadata), Status::kOk);

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
  ASSERT_EQ(device.program_page(PhysicalPage{1}, data, spare), Status::kNotErased);
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
  EXPECT_THROW(NandEmulator(4, {BlockNumber{4}}), std::invalid_argument);
  EXPECT_THROW(NandEmulator(4, {BlockNumber{0}}), std::invalid_argument);
}

}  // namespace
}  // namespace vincentftl
