#include "test_meas_struct.h"

#include <gtest/gtest.h>
#include "meas_func.h"


TEST(MeasurementStruct, DefaultConstruction) {
    measurement m;
    EXPECT_EQ(m.meas_num, 0);
    EXPECT_EQ(m.intp, 0);
    EXPECT_DOUBLE_EQ((double)m.fracp, 0.0);
}

TEST(MeasurementStruct, IsEmptyTrueWhenIntAndFracZero) {
    measurement m{10, 0, 0.0};
    EXPECT_TRUE(m.is_empty());
}

class MeasurementIsEmpty : public testing::TestWithParam<IsEmptyParams> {
};

TEST_P(MeasurementIsEmpty, IsNeverEmpty) {
    const auto &p = GetParam();
    measurement m{p.meas_num, p.intp, p.fracp};
    EXPECT_FALSE(m.is_empty());
}

INSTANTIATE_TEST_SUITE_P(
    MeasurementStructNotEmptyTests,
    MeasurementIsEmpty,
    testing::Values(
        // Non-zero intp
        IsEmptyParams{0, 1, static_cast<__float128>(0.0),},
        IsEmptyParams{0, -1, static_cast<__float128>(0.0),},
        IsEmptyParams{0, INT_MAX, static_cast<__float128>(0.0)},
        // Non-zero fracp
        IsEmptyParams{0, 0, static_cast<__float128>(0.5),},
        IsEmptyParams{0, 0, static_cast<__float128>(-0.5)},
        IsEmptyParams{0, 0, static_cast<__float128>(1.0),},
        // Both intp and fracp non-zero
        IsEmptyParams{0, 1, static_cast<__float128>(0.5)},
        // All fields non-zero
        IsEmptyParams{1, 1, static_cast<__float128>(0.5)}
    )
);

TEST(MeasurementStruct, IsValidFalseOnlyForNegTwo) {
    EXPECT_FALSE((measurement{-2, 0, {}}).is_valid());
}

TEST(MeasurementStruct, IsValidTrueForAllOtherMeasNums) {
    EXPECT_TRUE((measurement{0, 0, {}}).is_valid());
    EXPECT_TRUE((measurement{1, 0, {}}).is_valid());
    EXPECT_TRUE((measurement{-1, 0, {}}).is_valid());
    EXPECT_TRUE((measurement{255,0, {}}).is_valid());
}

class MeasurementToString : public testing::TestWithParam<ToStringParams> {
};

TEST_P(MeasurementToString, ReturnsExpectedString) {
    const auto &p = GetParam();
    measurement m{0, p.intp, p.fracp};
    EXPECT_EQ(m.to_string(), p.expected);
}

INSTANTIATE_TEST_SUITE_P(
    MeasurementToStringTests,
    MeasurementToString,
    testing::Values(
        ToStringParams{0, static_cast<__float128>(0.0), "0 0.000000000000000"},
        ToStringParams{1, static_cast<__float128>(0.5), "1 0.500000000000000"},
        ToStringParams{-1, static_cast<__float128>(0.5), "-1 0.500000000000000"},
        ToStringParams{0, static_cast<__float128>(0.123456789012345), "0 0.123456789012345"},
        ToStringParams{42, static_cast<__float128>(0.999999999999999), "42 0.999999999999999"}
    )
);

class MeasurementResolve : public testing::TestWithParam<ResolveParams> {
};

TEST_P(MeasurementResolve, ReturnsExpectedResult) {
    const auto &p = GetParam();
    measurement m{0, p.intp, p.fracp};
    m.resolve();
    EXPECT_EQ(m.intp, p.expected_intp);
    EXPECT_NEAR((double)m.fracp, p.expected_fracp, 1e-15);
}

INSTANTIATE_TEST_SUITE_P(
    MeasurementStructTests,
    MeasurementResolve,
    testing::Values(
        // No change needed
        ResolveParams{5, static_cast<__float128>(0.5), 5, 0.5},
        ResolveParams{3, static_cast<__float128>(0.0), 3, 0.0},
        ResolveParams{0, static_cast<__float128>(0.0), 0, 0.0},
        // Negative fracp borrows from intp
        ResolveParams{5, static_cast<__float128>(-0.3), 4, 0.7},
        ResolveParams{1, static_cast<__float128>(-0.0), 1, 0.0},
        ResolveParams{0, static_cast<__float128>(-0.5), -1, 0.5},
        // fracp >= 1.0 carries into intp
        ResolveParams{5, static_cast<__float128>(1.3), 6, 0.3},
        ResolveParams{0, static_cast<__float128>(1.0), 1, 0.0},
        // Resolve only ever changes the value by 1, even when more is required
        ResolveParams{5, static_cast<__float128>(2.7), 6, 1.7},
        // Boundary: fracp just under 1.0
        ResolveParams{5, static_cast<__float128>(0.999999999999999), 5, 0.999999999999999},
        // Negative intp
        ResolveParams{-3, static_cast<__float128>(0.5), -3, 0.5},
        ResolveParams{-3, static_cast<__float128>(-0.5), -4, 0.5}
    )
);

class MeasurementRound : public testing::TestWithParam<RoundParams> {
};

TEST_P(MeasurementRound, ReturnsExpectedResult) {
    const auto &p = GetParam();
    const measurement m{0, p.intp, p.fracp};
    EXPECT_EQ(m.round(), p.expected);
}

INSTANTIATE_TEST_SUITE_P(
    MeasurementStructTests,
    MeasurementRound,
    testing::Values(
        // Round down
        RoundParams{3, static_cast<__float128>(0.3), 3},
        RoundParams{3, static_cast<__float128>(0.1), 3},
        RoundParams{3, static_cast<__float128>(0.49), 3},
        // Round half up
        RoundParams{3, static_cast<__float128>(0.5), 4},
        RoundParams{0, static_cast<__float128>(0.5), 1},
        // Round up
        RoundParams{3, static_cast<__float128>(0.7), 4},
        RoundParams{3, static_cast<__float128>(0.99), 4},
        // Zero fracp
        RoundParams{3, static_cast<__float128>(0.0), 3},
        RoundParams{0, static_cast<__float128>(0.0), 0},
        // Negative intp, negative fracp
        RoundParams{-3, static_cast<__float128>(-0.5), -4},
        RoundParams{-3, static_cast<__float128>(-0.3), -3},
        RoundParams{-3, static_cast<__float128>(-0.7), -4},
        // Negative intp, positive fracp
        RoundParams{-3, static_cast<__float128>(0.3), -3},
        RoundParams{-3, static_cast<__float128>(0.5), -3}
    )
);

class MeasurementOperatorPlus : public testing::TestWithParam<OperatorPlusParams> {
};

TEST_P(MeasurementOperatorPlus, ReturnsExpectedResult) {
    const auto &p = GetParam();
    const measurement a{0, p.a_intp, p.a_fracp};
    const measurement b{0, p.b_intp, p.b_fracp};
    const measurement c = a + b;
    EXPECT_EQ(c.intp, p.expected_intp);
    EXPECT_NEAR((double)c.fracp, p.expected_fracp, 1e-15);
}

TEST_P(MeasurementOperatorPlus, DoesNotModifyOperands) {
    const auto &p = GetParam();
    measurement a{0, p.a_intp, p.a_fracp};
    measurement b{0, p.b_intp, p.b_fracp};
    [[maybe_unused]] const measurement c = a + b;
    EXPECT_EQ(a.intp, p.a_intp);
    EXPECT_EQ(b.intp, p.b_intp);
}

TEST_P(MeasurementOperatorPlus, ReturnsExpectedResultAssign) {
    const auto &p = GetParam();
    measurement a{0, p.a_intp, p.a_fracp};
    const measurement b{0, p.b_intp, p.b_fracp};
    a += b;
    EXPECT_EQ(a.intp, p.expected_intp);
    EXPECT_NEAR((double)a.fracp, p.expected_fracp, 1e-15);
}

INSTANTIATE_TEST_SUITE_P(
    MeasurementStructTests,
    MeasurementOperatorPlus,
    testing::Values(
        // Basic addition
        OperatorPlusParams{3, static_cast<__float128>(0.2), 2, static_cast<__float128>(0.5), 5, 0.7},
        // There is no inherent resolution
        OperatorPlusParams{3, static_cast<__float128>(0.7), 2, static_cast<__float128>(0.6), 5, 1.3},
        OperatorPlusParams{3, static_cast<__float128>(0.5), 2, static_cast<__float128>(0.5), 5, 1.0},
        // Zero operands
        OperatorPlusParams{0, static_cast<__float128>(0.0), 0, static_cast<__float128>(0.0), 0, 0.0},
        OperatorPlusParams{3, static_cast<__float128>(0.0), 0, static_cast<__float128>(0.0), 3, 0.0},
        // Negative intp
        OperatorPlusParams{-3, static_cast<__float128>(0.2), 2, static_cast<__float128>(0.5), -1, 0.7},
        // Both negative
        OperatorPlusParams{-3, static_cast<__float128>(-0.2), -2, static_cast<__float128>(-0.5), -5, -0.7}
    )
);
