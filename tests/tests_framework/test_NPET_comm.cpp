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
    EXPECT_EQ(INFINITE_OP, 9999);
}

TEST(Constants, PacketSizeValue) {
    EXPECT_EQ(NPET_comm::PACKET_SIZE, 13);
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
    const auto [firmware_version] = GetParam();
    EXPECT_NO_THROW({
        comm.set_FW_ver(firmware_version);
        EXPECT_EQ(comm.fw_version, firmware_version);
        });
}
