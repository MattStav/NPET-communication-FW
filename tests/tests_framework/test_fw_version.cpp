#include "fw_version.h"

#include <gtest/gtest.h>
#include <stdexcept>

TEST(FWVersion, GetValueReturnsConstructedVersion) {
    EXPECT_EQ(FWVersion(1).getValue(), 1);
    EXPECT_EQ(FWVersion(2).getValue(), 2);
    EXPECT_EQ(FWVersion(3).getValue(), 3);
}

TEST(FWVersion, DefaultConstructedValueIsZero) {
    EXPECT_EQ(FWVersion().getValue(), 0);
}

TEST(FWVersionGetMultiplier, FW1Returns2e8) {
    EXPECT_DOUBLE_EQ(static_cast<double>(FWVersion(FWVersion::ORIGINAL).getMultiplier()), 2e-8);
}

TEST(FWVersionGetMultiplier, FW2Returns1e8) {
    EXPECT_DOUBLE_EQ(static_cast<double>(FWVersion(FWVersion::AD_REVISION).getMultiplier()), 1e-8);
}

TEST(FWVersionGetMultiplier, FW3Returns1e8) {
    EXPECT_DOUBLE_EQ(static_cast<double>(FWVersion(FWVersion::VIRTUAL).getMultiplier()), 1e-8);
}

TEST(FWVersionGetMultiplier, FW2AndFW3AreEqual) {
    EXPECT_DOUBLE_EQ(static_cast<double>(FWVersion(FWVersion::AD_REVISION).getMultiplier()),
                     static_cast<double>(FWVersion(FWVersion::VIRTUAL).getMultiplier()));
}

TEST(FWVersionGetMultiplier, UnknownVersionThrows) {
    EXPECT_THROW(FWVersion(0).getMultiplier(), std::invalid_argument);
    EXPECT_THROW(FWVersion(4).getMultiplier(), std::invalid_argument);
    EXPECT_THROW(FWVersion(-1).getMultiplier(), std::invalid_argument);
}

TEST(FWVersionGetDescription, KnownVersions) {
    EXPECT_EQ(FWVersion(FWVersion::ORIGINAL).getDescription(), "Original");
    EXPECT_EQ(FWVersion(FWVersion::AD_REVISION).getDescription(), "Revision for NPET with AD component");
    EXPECT_EQ(FWVersion(FWVersion::VIRTUAL).getDescription(), "Virtual NPET");
}

TEST(FWVersionGetDescription, UnknownVersionThrows) {
    EXPECT_THROW(FWVersion(0).getDescription(), std::invalid_argument);
    EXPECT_THROW(FWVersion(4).getDescription(), std::invalid_argument);
    EXPECT_THROW(FWVersion(-1).getDescription(), std::invalid_argument);
}

TEST(FWVersionEquality, EqualWhenSameValue) {
    EXPECT_TRUE(FWVersion(1) == FWVersion(1));
    EXPECT_FALSE(FWVersion(1) != FWVersion(1));
}

TEST(FWVersionEquality, NotEqualWhenDifferentValue) {
    EXPECT_TRUE(FWVersion(1) != FWVersion(2));
    EXPECT_FALSE(FWVersion(1) == FWVersion(2));
}
