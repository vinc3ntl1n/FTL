#include "vincentftl/geometry.hpp"

#include <gtest/gtest.h>

#include "vincentftl/ftl.hpp"
#include "vincentftl/nand_device.hpp"

namespace vincentftl {
namespace {

TEST(Geometry, PhysicalPageSplitsIntoBlockAndPage) {
  const PhysicalPage page = PhysicalPage::in_block(BlockNumber{3}, 5);

  EXPECT_EQ(page.value, 3 * kPagesPerBlock + 5);
  EXPECT_EQ(page.block(), BlockNumber{3});
  EXPECT_EQ(page.page_in_block(), 5u);
}

TEST(Geometry, FtlOwnsBytesTwoToSevenOfEachSpareGroup) {
  int ftl_bytes = 0;
  for (std::size_t i = 0; i < kPageSpareBytes; ++i) {
    if (!is_reserved_spare_byte(i)) ++ftl_bytes;
  }
  EXPECT_EQ(ftl_bytes, 24);

  EXPECT_TRUE(is_reserved_spare_byte(0));
  EXPECT_FALSE(is_reserved_spare_byte(2));
  EXPECT_FALSE(is_reserved_spare_byte(7));
  EXPECT_TRUE(is_reserved_spare_byte(8));
  EXPECT_FALSE(is_reserved_spare_byte(18));
}

TEST(Geometry, ErasedSpareIsAllFF) {
  for (std::uint8_t byte : erased_spare()) EXPECT_EQ(byte, kErasedByte);
}

TEST(Geometry, StatusesPrintTheirNames) {
  EXPECT_EQ(testing::PrintToString(Status::kNotErased), "kNotErased");
  EXPECT_EQ(testing::PrintToString(FtlStatus::kNotWritten), "kNotWritten");
}

}  // namespace
}  // namespace vincentftl
