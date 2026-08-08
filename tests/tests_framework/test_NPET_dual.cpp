#include "test_NPET_dual.h"

#include <atomic>
#include <chrono>
#include <future>
#include <string>


TEST(NPETDualTest, ConstructsAndDestructsWithoutThrowing) {
    EXPECT_NO_THROW({
        const NPETDual dual;
        (void) dual;
        });
}

TEST_F(NPETDualFixture, DefaultConstructedStartIsNotOpen) {
    EXPECT_FALSE(one_.isOpen());
}

TEST_F(NPETDualFixture, DefaultConstructedStopIsNotOpen) {
    EXPECT_FALSE(two_.isOpen());
}

TEST_F(NPETDualFixture, DefaultConstructedStartFirmwareVersionIsZero) {
    EXPECT_EQ(one_.fw_version.getValue(), 0);
}

TEST_F(NPETDualFixture, DefaultConstructedStopFirmwareVersionIsZero) {
    EXPECT_EQ(two_.fw_version.getValue(), 0);
}

// start_/stop_ must be independent NPETComm instances, not aliases of the same underlying device.
TEST_F(NPETDualFixture, StartAndStopAreIndependentInstances) {
    one_.setFWVer(FWVersion::AD_REVISION);
    EXPECT_EQ(one_.fw_version.getValue(), FWVersion::AD_REVISION);
    EXPECT_EQ(two_.fw_version.getValue(), 0);
}


// --- executeBoth(): runs two callables concurrently, blocking until both complete ---

TEST_F(NPETDualFixture, ExecuteBothInvokesBothCallablesExactlyOnce) {
    std::atomic start_calls{0};
    std::atomic stop_calls{0};
    executeBoth([&] { ++start_calls; }, [&] { ++stop_calls; });
    EXPECT_EQ(start_calls.load(), 1);
    EXPECT_EQ(stop_calls.load(), 1);
}

TEST_F(NPETDualFixture, ExecuteBothBlocksUntilBothCallablesFinish) {
    std::atomic<bool> start_done{false};
    std::atomic<bool> stop_done{false};
    executeBoth(
        [&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            start_done = true;
        },
        [&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            stop_done = true;
        });
    EXPECT_TRUE(start_done.load());
    EXPECT_TRUE(stop_done.load());
}

// Proves the two callables actually run concurrently (released together off a shared latch, per
// executeBoth()'s doc comment) rather than one running to completion before the other starts: the
// start callable can only stop waiting once the stop callable has begun, so if they ran
// sequentially with start first, this would time out instead of observing ready.
TEST_F(NPETDualFixture, ExecuteBothRunsCallablesConcurrently) {
    std::promise<void> stop_started;
    const std::future<void> STOP_STARTED_FUTURE = stop_started.get_future();
    std::future_status start_wait_status{};
    executeBoth(
        [&] { start_wait_status = STOP_STARTED_FUTURE.wait_for(std::chrono::seconds(2)); },
        [&] { stop_started.set_value(); });
    EXPECT_EQ(start_wait_status, std::future_status::ready)
        << "stop callable never started before start callable finished waiting - "
           "the two callables are not running concurrently";
}


// --- switchStartStop(): logical START/STOP designation ---

TEST_F(NPETDualFixture, DefaultDesignationMapsStartToOneAndStopToTwo) {
    EXPECT_EQ(&startComm(), &one_);
    EXPECT_EQ(&stopComm(), &two_);
}

TEST_F(NPETDualFixture, SwitchStartStopSwapsDesignation) {
    switchStartStop();
    EXPECT_EQ(&startComm(), &two_);
    EXPECT_EQ(&stopComm(), &one_);
}

TEST_F(NPETDualFixture, SwitchStartStopTwiceRestoresOriginalDesignation) {
    switchStartStop();
    switchStartStop();
    EXPECT_EQ(&startComm(), &one_);
    EXPECT_EQ(&stopComm(), &two_);
}

TEST_F(NPETDualFixture, SwitchStartStopCalledOddNumberOfTimesLeavesDesignationSwapped) {
    for (int i = 0; i < 3; i++) {
        switchStartStop();
    }
    EXPECT_EQ(&startComm(), &two_);
    EXPECT_EQ(&stopComm(), &one_);
}

// switchStartStop() only relabels which accessor resolves to which instance - it must not touch
// one_/two_ themselves, since NPETComm is non-movable (see NPET_dual.h) and can't be physically
// swapped.
TEST_F(NPETDualFixture, SwitchStartStopRelabelsRatherThanMovingState) {
    one_.setFWVer(FWVersion::AD_REVISION);
    switchStartStop();
    EXPECT_EQ(one_.fw_version.getValue(), FWVersion::AD_REVISION)
        << "one_'s own state changed - switchStartStop() must not touch the underlying instances";
    EXPECT_EQ(two_.fw_version.getValue(), 0);
    EXPECT_EQ(stopComm().fw_version.getValue(), FWVersion::AD_REVISION)
        << "stopComm() should now resolve to one_, which carries the firmware version set above";
}


// --- DualMeasContext::toString() ---

class DualMeasContextToString : public testing::TestWithParam<DualToStringParams> {
};

TEST_P(DualMeasContextToString, ReturnsExpectedString) {
    const auto &p = GetParam();
    const DualMeasContext CTX{
        .num_of_meas = p.num_of_meas,
        .monitor_fn = p.has_monitor
                          ? decltype(DualMeasContext::monitor_fn){
                                [](MeasReader &, const MeasContext &, const Measurement &) {
                                }
                            }
                          : nullptr,
        .save_dir = p.save_dir ? std::optional<std::filesystem::path>(*p.save_dir) : std::nullopt,
        .start_channel = p.start_channel,
        .stop_channel = p.stop_channel,
    };
    EXPECT_EQ(CTX.toString(), p.expected);
}

INSTANTIATE_TEST_SUITE_P(
    DualMeasContextToStringTests,
    DualMeasContextToString,
    testing::Values(
        DualToStringParams{
            5, false, std::nullopt, Channel::CH1, Channel::CH1,
            "dual_meas_context{num_of_meas: 5, monitor_fn: null, save_dir: null, "
            "start_channel: 1, stop_channel: 1}"
        },
        DualToStringParams{
            10, true, std::nullopt, Channel::CH1, Channel::CH1,
            "dual_meas_context{num_of_meas: 10, monitor_fn: set, save_dir: null, "
            "start_channel: 1, stop_channel: 1}"
        },
        DualToStringParams{
            5, false, std::string("results"), Channel::CH1, Channel::CH1,
            "dual_meas_context{num_of_meas: 5, monitor_fn: null, save_dir: results, "
            "start_channel: 1, stop_channel: 1}"
        },
        DualToStringParams{
            1, false, std::nullopt, Channel::CH1, Channel::CH2,
            "dual_meas_context{num_of_meas: 1, monitor_fn: null, save_dir: null, "
            "start_channel: 1, stop_channel: 2}"
        },
        DualToStringParams{
            1, false, std::nullopt, Channel::CH2, Channel::CH2,
            "dual_meas_context{num_of_meas: 1, monitor_fn: null, save_dir: null, "
            "start_channel: 2, stop_channel: 2}"
        }
    )
);
