#include "test_meas_reader.h"

#include "gtest/gtest.h"


TEST(MeasContext, DefaultValues) {
    const MeasContext CTX{};
    EXPECT_EQ(CTX.num_of_meas, 5);
    EXPECT_EQ(CTX.channel, 1);
    EXPECT_FALSE(CTX.save_dir.has_value());
    EXPECT_FALSE(CTX.monitor_fn);
}

class MeasContextToStringTest : public testing::TestWithParam<MeasContextToStringParams> {};

TEST_P(MeasContextToStringTest, ContainsExpectedSubstring) {
    EXPECT_NE(GetParam().ctx.toString().find(GetParam().expected_substring), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(
    MeasContextTests,
    MeasContextToStringTest,
    testing::Values(
        // num_of_meas
        MeasContextToStringParams{{.num_of_meas = 10,  .monitor_fn = nullptr, .save_dir = std::nullopt, .channel = 1}, "num_of_meas: 10"},
        MeasContextToStringParams{{.num_of_meas = 1,   .monitor_fn = nullptr, .save_dir = std::nullopt, .channel = 1}, "num_of_meas: 1"},
        MeasContextToStringParams{{.num_of_meas = 255, .monitor_fn = nullptr, .save_dir = std::nullopt, .channel = 1}, "num_of_meas: 255"},
        // channel
        MeasContextToStringParams{{.num_of_meas = 5, .monitor_fn = nullptr, .save_dir = std::nullopt, .channel = 1}, "channel: 1"},
        MeasContextToStringParams{{.num_of_meas = 5, .monitor_fn = nullptr, .save_dir = std::nullopt, .channel = 2}, "channel: 2"},
        // monitor_fn
        MeasContextToStringParams{{.num_of_meas = 0, .monitor_fn = nullptr, .save_dir = std::nullopt, .channel = 1}, "monitor_fn: null"},
        MeasContextToStringParams{{.num_of_meas = 0, .monitor_fn = [](auto&&...) {}, .save_dir = std::nullopt, .channel = 1}, "monitor_fn: set"},
        // save_dir
        MeasContextToStringParams{{.num_of_meas = 0, .monitor_fn = nullptr, .save_dir = std::filesystem::path("C:/tmp/out"), .channel = 1}, "save_dir: C:/tmp/out"},
        MeasContextToStringParams{{.num_of_meas = 0, .monitor_fn = nullptr, .save_dir = std::nullopt, .channel = 1}, "save_dir: null"}
    )
);
