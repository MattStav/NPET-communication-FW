#include "test_NPET_dual.h"


TEST(NPETDualTest, ConstructsAndDestructsWithoutThrowing) {
    EXPECT_NO_THROW({
        const NPETDual dual;
        (void) dual;
        });
}

TEST_F(NPETDualFixture, DefaultConstructedStartIsNotOpen) {
    EXPECT_FALSE(start_.isOpen());
}

TEST_F(NPETDualFixture, DefaultConstructedStopIsNotOpen) {
    EXPECT_FALSE(stop_.isOpen());
}

TEST_F(NPETDualFixture, DefaultConstructedStartFirmwareVersionIsZero) {
    EXPECT_EQ(start_.fw_version.getValue(), 0);
}

TEST_F(NPETDualFixture, DefaultConstructedStopFirmwareVersionIsZero) {
    EXPECT_EQ(stop_.fw_version.getValue(), 0);
}
