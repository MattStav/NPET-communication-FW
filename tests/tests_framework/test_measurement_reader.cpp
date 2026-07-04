#include "test_measurement_reader.h"

#include "gtest/gtest.h"


TEST(MeasContext, DefaultValues) {
    const meas_context ctx{};
    EXPECT_EQ(ctx.num_of_meas, 5);
    EXPECT_EQ(ctx.channel, 1);
    EXPECT_FALSE(ctx.save);
    EXPECT_FALSE(ctx.monitor_fn);
}

class MeasContextToStringTest : public testing::TestWithParam<MeasContextToStringParams> {};

TEST_P(MeasContextToStringTest, ContainsExpectedSubstring) {
    EXPECT_NE(GetParam().ctx.to_string().find(GetParam().expected_substring), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(
    MeasContextTests,
    MeasContextToStringTest,
    testing::Values(
        // num_of_meas
        MeasContextToStringParams{{.num_of_meas = 10,  .monitor_fn = nullptr, .save = false, .channel = 1}, "num_of_meas: 10"},
        MeasContextToStringParams{{.num_of_meas = 1,   .monitor_fn = nullptr, .save = false, .channel = 1}, "num_of_meas: 1"},
        MeasContextToStringParams{{.num_of_meas = 255, .monitor_fn = nullptr, .save = false, .channel = 1}, "num_of_meas: 255"},
        // channel
        MeasContextToStringParams{{.num_of_meas = 5, .monitor_fn = nullptr, .save = false, .channel = 1}, "channel: 1"},
        MeasContextToStringParams{{.num_of_meas = 5, .monitor_fn = nullptr, .save = false, .channel = 2}, "channel: 2"},
        // monitor_fn
        MeasContextToStringParams{{.num_of_meas = 0, .monitor_fn = nullptr, .save = false, .channel = 1}, "monitor_fn: null"},
        MeasContextToStringParams{{.num_of_meas = 0, .monitor_fn = [](auto&&...) {}, .save = false, .channel = 1}, "monitor_fn: set"},
        // save flag
        MeasContextToStringParams{{.num_of_meas = 0, .monitor_fn = nullptr, .save = true,  .channel = 1}, "save: true"},
        MeasContextToStringParams{{.num_of_meas = 0, .monitor_fn = nullptr, .save = false, .channel = 1}, "save: false"}
    )
);