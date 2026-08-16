#include "test_meas_reader_dual.h"

#include <chrono>
#include <future>
#include <thread>


// --- DualMeasContext: defaults ---

TEST(DualMeasContext, DefaultValues) {
    const DualMeasContext CTX{};
    EXPECT_EQ(CTX.num_of_meas, 5);
    EXPECT_FALSE(CTX.monitor_fn);
    EXPECT_FALSE(CTX.save_dir.has_value());
    EXPECT_EQ(CTX.start_channel, Channel::CH1);
    EXPECT_EQ(CTX.stop_channel, Channel::CH1);
}


// --- DualMeasurement::toString() ---

TEST(DualMeasurementToString, ContainsBothMeasurementsToStrings) {
    constexpr DualMeasurement DM{
        .meas_start = Measurement{.meas_num = 3, .intp = 10, .fracp = 0.5},
        .meas_stop = Measurement{.meas_num = 3, .intp = 11, .fracp = 0.25},
    };
    const std::string STR = DM.toString();
    EXPECT_NE(STR.find(DM.meas_start.toString()), std::string::npos);
    EXPECT_NE(STR.find(DM.meas_stop.toString()), std::string::npos);
}


// --- DualMeasReader::matchMeasurement(): pairing by meas_num ---

TEST_F(DualMeasReaderFixture, StartThenStopSameMeasNumCombines) {
    constexpr Measurement START_MEAS{.meas_num = 1, .intp = 100, .fracp = 0.1};
    constexpr Measurement STOP_MEAS{.meas_num = 1, .intp = 200, .fracp = 0.2};
    matchMeasurement(true, START_MEAS);
    EXPECT_TRUE(for_monitor_q.empty()) << "should still be waiting for the matching stop measurement";
    matchMeasurement(false, STOP_MEAS);
    ASSERT_EQ(for_monitor_q.size(), 1U);
    EXPECT_EQ(for_monitor_q.front().meas_start.intp, START_MEAS.intp);
    EXPECT_EQ(for_monitor_q.front().meas_stop.intp, STOP_MEAS.intp);
}

// Matching must not depend on which leg's measurement arrives first.
TEST_F(DualMeasReaderFixture, StopThenStartSameMeasNumCombines) {
    constexpr Measurement START_MEAS{.meas_num = 2, .intp = 300, .fracp = 0.3};
    constexpr Measurement STOP_MEAS{.meas_num = 2, .intp = 400, .fracp = 0.4};
    matchMeasurement(false, STOP_MEAS);
    EXPECT_TRUE(for_monitor_q.empty()) << "should still be waiting for the matching start measurement";
    matchMeasurement(true, START_MEAS);
    ASSERT_EQ(for_monitor_q.size(), 1U);
    // Fields must land in the right slot regardless of arrival order
    EXPECT_EQ(for_monitor_q.front().meas_start.intp, START_MEAS.intp);
    EXPECT_EQ(for_monitor_q.front().meas_stop.intp, STOP_MEAS.intp);
}

TEST_F(DualMeasReaderFixture, DifferentMeasNumsStayPendingUnmatched) {
    matchMeasurement(true, Measurement{.meas_num = 1, .intp = 1});
    matchMeasurement(false, Measurement{.meas_num = 2, .intp = 2});
    EXPECT_TRUE(for_monitor_q.empty());
    EXPECT_EQ(pending_start_.size(), 1U);
    EXPECT_EQ(pending_stop_.size(), 1U);
}

TEST_F(DualMeasReaderFixture, MatchedPairIsRemovedFromPending) {
    matchMeasurement(true, Measurement{.meas_num = 5, .intp = 1});
    EXPECT_EQ(pending_start_.size(), 1U);
    matchMeasurement(false, Measurement{.meas_num = 5, .intp = 2});
    EXPECT_TRUE(pending_start_.empty());
    EXPECT_TRUE(pending_stop_.empty());
}

// meas_num 2's pair completes before meas_num 1's, so for_monitor_q must reflect completion
// order (2 before 1), not arrival order of the first-seen measurement of each pair.
TEST_F(DualMeasReaderFixture, MultipleInterleavedPairsAllMatchCorrectlyInCompletionOrder) {
    matchMeasurement(true, Measurement{.meas_num = 1, .intp = 11});
    matchMeasurement(true, Measurement{.meas_num = 2, .intp = 21});
    matchMeasurement(false, Measurement{.meas_num = 2, .intp = 22});
    matchMeasurement(false, Measurement{.meas_num = 1, .intp = 12});
    ASSERT_EQ(for_monitor_q.size(), 2U);
    EXPECT_EQ(for_monitor_q.front().meas_start.intp, 21);
    EXPECT_EQ(for_monitor_q.front().meas_stop.intp, 22);
    for_monitor_q.pop();
    EXPECT_EQ(for_monitor_q.front().meas_start.intp, 11);
    EXPECT_EQ(for_monitor_q.front().meas_stop.intp, 12);
}


// --- DualMeasReader::finishLeg() ---

TEST_F(DualMeasReaderFixture, FinishLegOnceDoesNotSetStopSign) {
    finishLeg();
    EXPECT_FALSE(stop_sign_.load());
}

TEST_F(DualMeasReaderFixture, FinishLegTwiceSetsStopSign) {
    finishLeg();
    finishLeg();
    EXPECT_TRUE(stop_sign_.load());
}

// Leftover unmatched measurements are only ever warned about, never silently discarded, so the
// pending maps still reflect them after both legs finish.
TEST_F(DualMeasReaderFixture, FinishLegDoesNotClearLeftoverPendingMeasurements) {
    matchMeasurement(true, Measurement{.meas_num = 9, .intp = 1});
    finishLeg();
    finishLeg();
    EXPECT_EQ(pending_start_.size(), 1U);
}

// --- DualMeasReader::grabMeasurement() ---

TEST_F(DualMeasReaderFixture, GrabMeasurementReturnsQueuedPairImmediately) {
    matchMeasurement(true, Measurement{.meas_num = 1, .intp = 42});
    matchMeasurement(false, Measurement{.meas_num = 1, .intp = 43});
    const std::optional<DualMeasurement> RESULT = grabMeasurement();
    ASSERT_TRUE(RESULT.has_value());
    EXPECT_EQ(RESULT->meas_start.intp, 42);
    EXPECT_EQ(RESULT->meas_stop.intp, 43);
}

TEST_F(DualMeasReaderFixture, GrabMeasurementFifoOrdersMultiplePairs) {
    matchMeasurement(true, Measurement{.meas_num = 1, .intp = 1});
    matchMeasurement(false, Measurement{.meas_num = 1, .intp = 2});
    matchMeasurement(true, Measurement{.meas_num = 2, .intp = 3});
    matchMeasurement(false, Measurement{.meas_num = 2, .intp = 4});
    EXPECT_EQ(grabMeasurement()->meas_start.intp, 1);
    EXPECT_EQ(grabMeasurement()->meas_start.intp, 3);
}

// Once both legs are finished and the queue is empty, grabMeasurement() must return promptly
// (no data ever coming) rather than blocking indefinitely.
TEST_F(DualMeasReaderFixture, GrabMeasurementReturnsNulloptOnceBothLegsFinishedAndQueueEmpty) {
    finishLeg();
    finishLeg();
    EXPECT_EQ(grabMeasurement(), std::nullopt);
}

// Proves grabMeasurement() actually blocks waiting for data rather than returning nullopt right
// away: it can only return once matchMeasurement() (called from another thread, after a short
// delay) has produced a completed pair.
TEST_F(DualMeasReaderFixture, GrabMeasurementBlocksUntilMatchingPairArrives) {
    std::promise<void> grabber_started;
    const std::future<void> GRABBER_STARTED_FUTURE = grabber_started.get_future();
    std::optional<DualMeasurement> result;
    std::jthread grabber([&] {
        grabber_started.set_value();
        result = grabMeasurement();
    });
    GRABBER_STARTED_FUTURE.wait();
    // Give the grabber thread a moment to actually enter its wait loop before data appears
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(result.has_value()) << "grabMeasurement() returned before any data was pushed";
    matchMeasurement(true, Measurement{.meas_num = 1, .intp = 7});
    matchMeasurement(false, Measurement{.meas_num = 1, .intp = 8});
    grabber.join();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->meas_start.intp, 7);
    EXPECT_EQ(result->meas_stop.intp, 8);
}

// Proves grabMeasurement() also unblocks (with nullopt) when both legs finish while it is
// waiting, rather than only ever noticing dual_stop_sign_ on a fresh call.
TEST_F(DualMeasReaderFixture, GrabMeasurementUnblocksWithNulloptWhenLegsFinishWhileWaiting) {
    std::optional<DualMeasurement> result{DualMeasurement{}}; // pre-filled so has_value() is meaningful below
    std::jthread grabber([&] { result = grabMeasurement(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    finishLeg();
    finishLeg();
    grabber.join();
    EXPECT_FALSE(result.has_value());
}


// --- DualMeasReader::unmatchedMeasurements() ---

TEST_F(DualMeasReaderFixture, NoLeftoversReturnsEmptyResult) {
    const UnmatchedMeasurements RESULT = unmatchedMeasurements();
    EXPECT_TRUE(RESULT.start.empty());
    EXPECT_TRUE(RESULT.stop.empty());
    EXPECT_EQ(RESULT.size(), 0U);
}

TEST_F(DualMeasReaderFixture, ReturnsOnlyStartLegLeftovers) {
    matchMeasurement(true, Measurement{.meas_num = 1, .intp = 11});
    matchMeasurement(true, Measurement{.meas_num = 2, .intp = 12});
    const auto [start, stop] = unmatchedMeasurements();
    EXPECT_EQ(start.size(), 2U);
    EXPECT_TRUE(stop.empty());
}

TEST_F(DualMeasReaderFixture, ReturnsOnlyStopLegLeftovers) {
    matchMeasurement(false, Measurement{.meas_num = 1, .intp = 21});
    const auto [start, stop] = unmatchedMeasurements();
    EXPECT_TRUE(start.empty());
    ASSERT_EQ(stop.size(), 1U);
    EXPECT_EQ(stop.front().intp, 21);
}

TEST_F(DualMeasReaderFixture, ReturnsLeftoversFromBothLegs) {
    matchMeasurement(true, Measurement{.meas_num = 1, .intp = 31});
    matchMeasurement(false, Measurement{.meas_num = 2, .intp = 32});
    const UnmatchedMeasurements RESULT = unmatchedMeasurements();
    ASSERT_EQ(RESULT.start.size(), 1U);
    ASSERT_EQ(RESULT.stop.size(), 1U);
    EXPECT_EQ(RESULT.start.front().intp, 31);
    EXPECT_EQ(RESULT.stop.front().intp, 32);
    EXPECT_EQ(RESULT.size(), 2U);
}

// Measurements that already found their match must not reappear as leftovers.
TEST_F(DualMeasReaderFixture, MatchedPairsAreExcludedFromLeftovers) {
    matchMeasurement(true, Measurement{.meas_num = 1, .intp = 41});
    matchMeasurement(false, Measurement{.meas_num = 1, .intp = 42}); // matches and clears pending
    matchMeasurement(true, Measurement{.meas_num = 2, .intp = 43}); // stays unmatched
    const auto [start, stop] = unmatchedMeasurements();
    ASSERT_EQ(start.size(), 1U);
    EXPECT_EQ(start.front().intp, 43);
    EXPECT_TRUE(stop.empty());
}

// Calling unmatchedMeasurements() must not itself mutate the pending maps.
TEST_F(DualMeasReaderFixture, IsIdempotentAcrossRepeatedCalls) {
    matchMeasurement(true, Measurement{.meas_num = 1, .intp = 51});
    EXPECT_EQ(unmatchedMeasurements().start.size(), 1U);
    EXPECT_EQ(unmatchedMeasurements().start.size(), 1U);
}
