#include "vincentftl/page_ftl.hpp"

#include <gtest/gtest.h>

#include "vincentftl/nand_emulator.hpp"

namespace vincentftl {
namespace {

// Page with different bytes for each seed.
PageData make_page(std::uint8_t seed) {
  PageData data;
  for (std::size_t i = 0; i < data.size(); ++i) {
    data[i] = static_cast<std::uint8_t>(seed + i);
  }
  return data;
}

// Real emulator, but we can make programs or reads fail.
class FailingDevice final : public NandDevice {
 public:
  explicit FailingDevice(NandEmulator& real) : real_(real) {}

  bool fail_program = false;
  bool fail_read = false;

  std::uint32_t block_count() const override { return real_.block_count(); }
  Status read_page(PhysicalPage page, PageData& data, PageSpare& spare) override {
    return fail_read ? Status::kBadAddress : real_.read_page(page, data, spare);
  }
  Status read_spare(PhysicalPage page, PageSpare& spare) override {
    return real_.read_spare(page, spare);
  }
  Status program_page(PhysicalPage page, const PageData& data, const PageSpare& spare) override {
    return fail_program ? Status::kProgramFailed : real_.program_page(page, data, spare);
  }
  Status erase_block(BlockNumber block) override { return real_.erase_block(block); }
  bool is_factory_bad(BlockNumber block) override { return real_.is_factory_bad(block); }

 private:
  NandEmulator& real_;
};

TEST(PageFtl, CapacityIsAllPages) {
  NandEmulator device(4);
  PageFtl ftl(device);

  EXPECT_EQ(ftl.capacity(), 4 * kPagesPerBlock);
}

TEST(PageFtl, CapacitySkipsBadBlocks) {
  NandEmulator device(8, {BlockNumber{2}, BlockNumber{5}});
  PageFtl ftl(device);

  EXPECT_EQ(ftl.capacity(), 6 * kPagesPerBlock);
}

TEST(PageFtl, ReadReturnsWhatWasWritten) {
  NandEmulator device(4);
  PageFtl ftl(device);
  ASSERT_EQ(ftl.write(LogicalPage{3}, make_page(1)), FtlStatus::kOk);

  PageData out;
  ASSERT_EQ(ftl.read(LogicalPage{3}, out), FtlStatus::kOk);
  EXPECT_EQ(out, make_page(1));
}

TEST(PageFtl, ReadOfUnwrittenPageFails) {
  NandEmulator device(4);
  PageFtl ftl(device);

  PageData out;
  EXPECT_EQ(ftl.read(LogicalPage{0}, out), FtlStatus::kNotWritten);
}

TEST(PageFtl, PagesWrittenOutOfOrderStayApart) {
  NandEmulator device(4);
  PageFtl ftl(device);
  ASSERT_EQ(ftl.write(LogicalPage{200}, make_page(1)), FtlStatus::kOk);
  ASSERT_EQ(ftl.write(LogicalPage{5}, make_page(2)), FtlStatus::kOk);
  ASSERT_EQ(ftl.write(LogicalPage{64}, make_page(3)), FtlStatus::kOk);

  PageData out;
  ASSERT_EQ(ftl.read(LogicalPage{5}, out), FtlStatus::kOk);
  EXPECT_EQ(out, make_page(2));
  ASSERT_EQ(ftl.read(LogicalPage{64}, out), FtlStatus::kOk);
  EXPECT_EQ(out, make_page(3));
  ASSERT_EQ(ftl.read(LogicalPage{200}, out), FtlStatus::kOk);
  EXPECT_EQ(out, make_page(1));
}

TEST(PageFtl, OverwriteReturnsNewData) {
  NandEmulator device(4);
  PageFtl ftl(device);
  ASSERT_EQ(ftl.write(LogicalPage{7}, make_page(1)), FtlStatus::kOk);
  ASSERT_EQ(ftl.write(LogicalPage{7}, make_page(2)), FtlStatus::kOk);

  PageData out;
  ASSERT_EQ(ftl.read(LogicalPage{7}, out), FtlStatus::kOk);
  EXPECT_EQ(out, make_page(2));
}

TEST(PageFtl, OverwriteUsesANewPhysicalPage) {
  // NAND can't program the same page twice without an erase.
  NandEmulator device(4);
  PageFtl ftl(device);

  ASSERT_EQ(ftl.write(LogicalPage{0}, make_page(1)), FtlStatus::kOk);
  ASSERT_EQ(ftl.write(LogicalPage{0}, make_page(2)), FtlStatus::kOk);

  EXPECT_EQ(device.pages_programmed(), 2u);
}

TEST(PageFtl, LastPageWorks) {
  NandEmulator device(4);
  PageFtl ftl(device);
  const LogicalPage last{ftl.capacity() - 1};

  ASSERT_EQ(ftl.write(last, make_page(1)), FtlStatus::kOk);

  PageData out;
  ASSERT_EQ(ftl.read(last, out), FtlStatus::kOk);
  EXPECT_EQ(out, make_page(1));
}

TEST(PageFtl, WritePastEndIsBadAddress) {
  NandEmulator device(4);
  PageFtl ftl(device);

  EXPECT_EQ(ftl.write(LogicalPage{ftl.capacity()}, make_page(1)), FtlStatus::kBadAddress);
  EXPECT_EQ(device.pages_programmed(), 0u);
}

TEST(PageFtl, ReadPastEndIsBadAddress) {
  NandEmulator device(4);
  PageFtl ftl(device);

  PageData out;
  EXPECT_EQ(ftl.read(LogicalPage{ftl.capacity()}, out), FtlStatus::kBadAddress);
}

TEST(PageFtl, NeverWritesToBadBlocks) {
  // The emulator fails programs on bad blocks, so a full fill means we skipped them.
  NandEmulator device(6, {BlockNumber{1}, BlockNumber{4}});
  PageFtl ftl(device);

  for (std::uint32_t i = 0; i < ftl.capacity(); ++i) {
    ASSERT_EQ(ftl.write(LogicalPage{i}, make_page(static_cast<std::uint8_t>(i))), FtlStatus::kOk);
  }

  PageData out;
  ASSERT_EQ(ftl.read(LogicalPage{ftl.capacity() - 1}, out), FtlStatus::kOk);
  EXPECT_EQ(out, make_page(static_cast<std::uint8_t>(ftl.capacity() - 1)));
}

TEST(PageFtl, BadBlockAtTheEndIsSkipped) {
  NandEmulator device(3, {BlockNumber{2}});
  PageFtl ftl(device);

  for (std::uint32_t i = 0; i < ftl.capacity(); ++i) {
    ASSERT_EQ(ftl.write(LogicalPage{i}, make_page(1)), FtlStatus::kOk);
  }
}

TEST(PageFtl, NoSpaceWhenAllPagesAreUsed) {
  // No garbage collection yet, so an overwrite uses up a new page.
  NandEmulator device(2);
  PageFtl ftl(device);
  for (std::uint32_t i = 0; i < ftl.capacity(); ++i) {
    ASSERT_EQ(ftl.write(LogicalPage{i}, make_page(1)), FtlStatus::kOk);
  }

  EXPECT_EQ(ftl.write(LogicalPage{0}, make_page(2)), FtlStatus::kNoSpace);
}

TEST(PageFtl, NoSpaceKeepsOldData) {
  NandEmulator device(2);
  PageFtl ftl(device);
  for (std::uint32_t i = 0; i < ftl.capacity(); ++i) {
    ASSERT_EQ(ftl.write(LogicalPage{i}, make_page(1)), FtlStatus::kOk);
  }
  ASSERT_EQ(ftl.write(LogicalPage{0}, make_page(2)), FtlStatus::kNoSpace);

  PageData out;
  ASSERT_EQ(ftl.read(LogicalPage{0}, out), FtlStatus::kOk);
  EXPECT_EQ(out, make_page(1));
}

TEST(PageFtl, ProgramFailureIsDeviceError) {
  NandEmulator real(4);
  FailingDevice device(real);
  PageFtl ftl(device);

  device.fail_program = true;
  EXPECT_EQ(ftl.write(LogicalPage{0}, make_page(1)), FtlStatus::kDeviceError);

  PageData out;
  EXPECT_EQ(ftl.read(LogicalPage{0}, out), FtlStatus::kNotWritten);
}

TEST(PageFtl, FailedOverwriteKeepsOldData) {
  NandEmulator real(4);
  FailingDevice device(real);
  PageFtl ftl(device);
  ASSERT_EQ(ftl.write(LogicalPage{0}, make_page(1)), FtlStatus::kOk);

  device.fail_program = true;
  ASSERT_EQ(ftl.write(LogicalPage{0}, make_page(2)), FtlStatus::kDeviceError);

  PageData out;
  ASSERT_EQ(ftl.read(LogicalPage{0}, out), FtlStatus::kOk);
  EXPECT_EQ(out, make_page(1));
}

TEST(PageFtl, ReadFailureIsDeviceError) {
  NandEmulator real(4);
  FailingDevice device(real);
  PageFtl ftl(device);
  ASSERT_EQ(ftl.write(LogicalPage{0}, make_page(1)), FtlStatus::kOk);

  device.fail_read = true;
  PageData out;
  EXPECT_EQ(ftl.read(LogicalPage{0}, out), FtlStatus::kDeviceError);
}

}  // namespace
}  // namespace vincentftl
