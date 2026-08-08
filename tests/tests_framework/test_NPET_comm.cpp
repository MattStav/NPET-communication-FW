#include "test_NPET_comm.h"

#include <gtest/gtest.h>
#include <string>
#include <gmock/gmock.h>
#include <boost/asio.hpp>
#include <memory>
#include <vector>

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::Invoke;
using ::testing::AtLeast;
using ::testing::Sequence;


TEST(Constants, InfiniteOpValue) {
    EXPECT_EQ(INFINITE_OPERATION, 9999);
}

TEST(Constants, PacketSizeValue) {
    EXPECT_EQ(MEASUREMENT_PACKET_SIZE, 13);
}

TEST_F(NPETCommFixture, DefaultConstructedFirmwareVersionIsZero) {
    EXPECT_EQ(comm.fw_version.getValue(), 0);
}

// NPETComm inherits SerialMachine's closed-by-default state; the destructor
// relies on isOpen() to decide whether to talk to the device, so this must
// hold for the fixture (and every other test built on it) to be hermetic.
TEST_F(NPETCommFixture, DefaultConstructedIsNotOpen) {
    EXPECT_FALSE(comm.isOpen());
}


class SetFirmwareVersionTest : public NPETCommFixture,
                               public ::testing::WithParamInterface<FirmwareVersionTestParams> {
};

INSTANTIATE_TEST_SUITE_P(
    FirmwareVersions,
    SetFirmwareVersionTest,
    ::testing::Values(
        FirmwareVersionTestParams{1},
        FirmwareVersionTestParams{2},
        FirmwareVersionTestParams{3}
    )
);

TEST_P(SetFirmwareVersionTest, SetValidAndInvalidFirmwareVersions) {
    const auto [FIRMWARE_VERSION] = GetParam();
    EXPECT_NO_THROW({
        comm.setFWVer(FWVersion(FIRMWARE_VERSION));
        EXPECT_EQ(comm.fw_version, FWVersion(FIRMWARE_VERSION));
        });
}
