// Tests of the given parts of W25nDevice, and of the pretend chip the driver tests run against.

#include "vincentftl/w25n_device.hpp"

#include <gtest/gtest.h>

#include "fake_w25n.hpp"

namespace vincentftl {
namespace {

TEST(W25nDevice, ReadsThePowerUpValuesOfTheStatusRegisters) {
  FakeW25n bus;
  W25nDevice device(bus, 8, {});

  EXPECT_EQ(device.read_register(W25nDevice::kProtectionRegister), 0x7C);
  EXPECT_EQ(device.read_register(W25nDevice::kConfigRegister), 0x18);
  EXPECT_EQ(device.read_register(W25nDevice::kStatusRegister), 0x00);
  EXPECT_TRUE(bus.mistakes().empty()) << bus.mistakes().front();
}

TEST(W25nDevice, WritesARegister) {
  FakeW25n bus;
  W25nDevice device(bus, 8, {});

  device.write_register(W25nDevice::kProtectionRegister, 0x00);

  EXPECT_EQ(device.read_register(W25nDevice::kProtectionRegister), 0x00);
  EXPECT_TRUE(bus.mistakes().empty()) << bus.mistakes().front();
}

TEST(W25nDevice, WriteEnableSetsTheLatch) {
  FakeW25n bus;
  W25nDevice device(bus, 8, {});

  device.write_enable();

  EXPECT_EQ(device.read_register(W25nDevice::kStatusRegister) & W25nDevice::kWriteEnabled,
            W25nDevice::kWriteEnabled);
}

TEST(W25nDevice, WaitReadyWaitsUntilTheChipIsNoLongerBusy) {
  FakeW25n bus;
  W25nDevice device(bus, 8, {});
  bus.transfer({0x13, 0x00, 0x00, 0x00});  // Page Data Read keeps the chip busy for a moment

  const auto status = device.wait_ready();

  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(*status & W25nDevice::kBusy, 0);
  EXPECT_GT(bus.commands().size(), 2u) << "it should have asked more than once";
}

TEST(W25nDevice, WaitReadyGivesUpWhenTheChipStaysBusy) {
  FakeW25n bus;
  W25nDevice device(bus, 8, {});
  bus.stay_busy();

  EXPECT_FALSE(device.wait_ready().has_value());
}

TEST(W25nDevice, KnowsTheFactoryBadBlocksItWasGiven) {
  FakeW25n bus;
  W25nDevice device(bus, 8, {BlockNumber{3}, BlockNumber{500}});

  EXPECT_EQ(device.block_count(), 8u);
  EXPECT_TRUE(device.is_factory_bad(BlockNumber{3}));
  EXPECT_FALSE(device.is_factory_bad(BlockNumber{2}));
  // Block 500 is past the 8 blocks this device uses, so it's left out.
  EXPECT_FALSE(device.is_factory_bad(BlockNumber{500}));
}

TEST(FakeW25n, AnswersWithTheW25n01gvId) {
  FakeW25n bus;

  const auto reply = bus.transfer({0x9F, 0x00, 0x00, 0x00, 0x00});

  EXPECT_EQ(reply[2], 0xEF);
  EXPECT_EQ(reply[3], 0xAA);
  EXPECT_EQ(reply[4], 0x21);
}

TEST(FakeW25n, NoticesACommandSentWhileBusy) {
  FakeW25n bus;
  bus.transfer({0x13, 0x00, 0x00, 0x00});  // busy for a moment

  bus.transfer({0x03, 0x00, 0x00, 0x00, 0x00});  // reading without waiting first

  EXPECT_EQ(bus.mistakes().size(), 1u);
}

}  // namespace
}  // namespace vincentftl
