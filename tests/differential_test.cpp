#include "vincentftl/differential.hpp"

#include <gtest/gtest.h>

#include "vincentftl/nand_emulator.hpp"
#include "vincentftl/reference_ftl.hpp"

namespace vincentftl {
namespace {
// An FTL with a bug on purpose: it answers "ok" to every 50th write but forgets the data.
class ForgetfulFtl final : public Ftl {
 public:
  explicit ForgetfulFtl(std::uint32_t capacity) : inner_(capacity) {}

  std::uint32_t capacity() const override { return inner_.capacity(); }
  FtlStatus write(LogicalPage page, const PageData& data) override {
    ++writes_;
    if (writes_ % 50 == 0) return FtlStatus::kOk;
    return inner_.write(page, data);
  }
  FtlStatus read(LogicalPage page, PageData& data) override { return inner_.read(page, data); }
  FtlStatus mount() override { return FtlStatus::kOk; }

 private:
  ReferenceFtl inner_;
  std::uint32_t writes_ = 0;
};

TEST(ReferenceFtl, ReadsBackWhatWasWritten) {
  ReferenceFtl subject(64);
  PageData written;
  written.fill(0xA1);
  ASSERT_EQ(subject.write(LogicalPage{7}, written), FtlStatus::kOk);

  PageData read_back;
  ASSERT_EQ(subject.read(LogicalPage{7}, read_back), FtlStatus::kOk);

  EXPECT_EQ(read_back, written);
}

TEST(ReferenceFtl, OverwriteReturnsTheNewestData) {
  ReferenceFtl reference(64);
  PageData first_write;
  first_write.fill(0xA1);
  ASSERT_EQ(reference.write(LogicalPage{7}, first_write), FtlStatus::kOk);

  PageData second_write;
  second_write.fill(0xB2);
  ASSERT_EQ(reference.write(LogicalPage{7}, second_write), FtlStatus::kOk);

  PageData read_back;
  ASSERT_EQ(reference.read(LogicalPage{7}, read_back), FtlStatus::kOk);

  EXPECT_EQ(read_back, second_write);
}

TEST(ReferenceFtl, ReadOfANeverWrittenPageReturnsNotWritten) {
  ReferenceFtl reference(64);
  PageData read_back;
  ASSERT_EQ(reference.read(LogicalPage{7}, read_back), FtlStatus::kNotWritten);
}

TEST(ReferenceFtl, AddressPastCapacityReturnsBadAddress) {
  ReferenceFtl reference(64);
  PageData written;
  written.fill(0xA1);
  ASSERT_EQ(reference.write(LogicalPage{64}, written), FtlStatus::kBadAddress);

  PageData read_back;
  ASSERT_EQ(reference.read(LogicalPage{64}, read_back), FtlStatus::kBadAddress);
}

TEST(Differential, CatchesAnFtlThatForgetsWrites) {
  ForgetfulFtl subject(64);
  ReferenceFtl reference(64);

  const auto mismatch = run_differential(subject, reference, 1, 10'000);

  EXPECT_TRUE(mismatch.has_value());
}

TEST(Differential, TwoReferenceModelsAlwaysAgree) {
  ReferenceFtl reference1(64);
  ReferenceFtl reference2(64);

  const auto mismatch = run_differential(reference1, reference2, 1, 10'000);
  EXPECT_FALSE(mismatch.has_value()) << mismatch.value_or("");
}

TEST(Differential, ReportsDifferentCapacities) {
  ReferenceFtl reference1(64);
  ReferenceFtl reference2(32);
  const auto mismatch = run_differential(reference1, reference2, 1, 10'000);
  ASSERT_EQ(mismatch.value(), "Ftl capacity mismatch\n");
}
}  // namespace
}  // namespace vincentftl
