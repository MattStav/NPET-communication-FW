#include "test_fw_version.h"

#include <climits>
#include <gtest/gtest.h>
#include <stdexcept>

#include "fw_version.h"
#include "meas_func.h" // float128ToString

class KnownVersionTest : public testing::TestWithParam<KnownVersionParams> {
};

TEST_P(KnownVersionTest, GetValueReturnsConstructedVersion) {
    EXPECT_EQ(FWVersion(GetParam().version).getValue(), GetParam().version);
}

// Compares the raw __float128 value directly rather than casting to double first: a cast to
// double rounds away everything past ~15-17 significant decimal digits, which would silently
// hide a regression in the low-order bits of a 128-bit quad value.
TEST_P(KnownVersionTest, GetMultiplierMatchesExpectedFloat128Value) {
    const __float128 ACTUAL = FWVersion(GetParam().version).getMultiplier();
    EXPECT_TRUE(ACTUAL == GetParam().expected_multiplier)
        << "Actual: " << float128ToString(ACTUAL)
        << ", Expected: " << float128ToString(GetParam().expected_multiplier);
}

// Cross-checks the same value through the quad-precision formatting path used elsewhere in the
// codebase (decodeMeasurementSet/encodeMeasurementSet), so a formatting-level precision loss
// would also be caught.
TEST_P(KnownVersionTest, GetMultiplierFormatsToExpected15DigitString) {
    EXPECT_EQ(float128ToString(FWVersion(GetParam().version).getMultiplier()),
              float128ToString(GetParam().expected_multiplier));
}

TEST_P(KnownVersionTest, GetDescriptionReturnsExpected) {
    EXPECT_EQ(FWVersion(GetParam().version).getDescription(), GetParam().expected_description);
}

INSTANTIATE_TEST_SUITE_P(
    FWVersions,
    KnownVersionTest,
    testing::Values(
        KnownVersionParams{FWVersion::ORIGINAL, static_cast<__float128>(0.00000002), "Original"},
        KnownVersionParams{
        FWVersion::AD_REVISION, static_cast<__float128>(0.00000001), "Revision for NPET with AD component"
        },
        KnownVersionParams{FWVersion::VIRTUAL, static_cast<__float128>(0.00000001), "Virtual NPET"}
    )
);

TEST(FWVersion, DefaultConstructedValueIsZero) {
    // The default constructor deliberately bypasses validation (unlike FWVersion(int)): it
    // represents the "not yet set" state NPETComm starts in before setFWVer/detectFWVer runs.
    EXPECT_EQ(FWVersion().getValue(), 0);
}

TEST(FWVersion, DefaultConstructedThrowsOnGetMultiplier) {
    EXPECT_THROW((void) FWVersion().getMultiplier(), std::invalid_argument);
}

TEST(FWVersion, DefaultConstructedThrowsOnGetDescription) {
    EXPECT_THROW((void) FWVersion().getDescription(), std::invalid_argument);
}

// FW2 and FW3 are documented as sharing a multiplier; EXPECT_DOUBLE_EQ would only prove that
// down to double precision, so compare the full float128 values directly.
TEST(FWVersionGetMultiplier, FW2AndFW3AreBitIdenticalAtFloat128Precision) {
    EXPECT_TRUE(FWVersion(FWVersion::AD_REVISION).getMultiplier() == FWVersion(FWVersion::VIRTUAL).getMultiplier());
}

class UnknownVersionTest : public testing::TestWithParam<int> {
};

// Validation happens at construction time, so an object holding an unknown version can
// never exist; verify the constructor itself rejects it rather than getMultiplier/getDescription.
TEST_P(UnknownVersionTest, ConstructorThrows) {
    EXPECT_THROW((FWVersion{GetParam()}), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
    UnknownFWVersions,
    UnknownVersionTest,
    testing::Values(0, 4, -1, INT_MIN, INT_MAX)
);

class EqualityTest : public testing::TestWithParam<EqualityParams> {
};

TEST_P(EqualityTest, OperatorEqualsMatchesExpected) {
    const auto &p = GetParam();
    EXPECT_EQ(FWVersion(p.a) == FWVersion(p.b), p.expected_equal);
}

TEST_P(EqualityTest, OperatorNotEqualsIsInverseOfEquals) {
    const auto &p = GetParam();
    EXPECT_EQ(FWVersion(p.a) != FWVersion(p.b), !p.expected_equal);
}

INSTANTIATE_TEST_SUITE_P(
    FWVersionEquality,
    EqualityTest,
    testing::Values(
        // Same value
        EqualityParams{1, 1, true},
        EqualityParams{2, 2, true},
        EqualityParams{3, 3, true},
        // Different values
        EqualityParams{1, 2, false},
        EqualityParams{1, 3, false},
        EqualityParams{2, 3, false}
    )
);

TEST(FWVersionEquality, DefaultConstructedInstancesAreEqual) {
    EXPECT_TRUE(FWVersion() == FWVersion());
    EXPECT_FALSE(FWVersion() != FWVersion());
}

class DefaultVsValidEqualityTest : public testing::TestWithParam<int> {
};

TEST_P(DefaultVsValidEqualityTest, DefaultConstructedIsNeverEqualToAValidVersion) {
    EXPECT_FALSE(FWVersion() == FWVersion(GetParam()));
    EXPECT_TRUE(FWVersion() != FWVersion(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    ValidFWVersions,
    DefaultVsValidEqualityTest,
    testing::Values(FWVersion::ORIGINAL, FWVersion::AD_REVISION, FWVersion::VIRTUAL)
);
