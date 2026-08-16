#include "test_workflow_fixture.h"

#include <atomic>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <future>
#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>
#include <string>
#include <vector>
#include <windows.h>

#include "meas_func.h"

// Tests below exercise NPETComm's measurement-reading surface (both the single-shot and the
// batch/streaming paths) over the live connection brought up by FrameworkWorkflowFixture

namespace fs = std::filesystem;
using ::testing::MatchesRegex;

// Matches Measurement::toString() / float128ToString()'s fixed 15-decimal-digit format, e.g. "0 1.234500000000000".
constexpr auto MEASUREMENT_LINE_PATTERN =
        R"(-*\d\d* -*\d\d*\.\d\d\d\d\d\d\d\d\d\d\d\d\d\d\d)";

// All measurement reads decode using npet_.fw_version's multiplier, which must match the
// multiplier the VM encoded with (VIRTUAL) or decoded values come out meaningless.
// Make sure the client properly detects the correct fw_version.
class MeasurementWorkflowFixture : public FrameworkWorkflowFixture {
protected:
    void SetUp() override {
        if (client) {
            client->detectFWVer();
            EXPECT_EQ(client->getFWVer(), FWVersion(FWVersion::VIRTUAL));
        }
    }
};


// --- Single measurement: both readSingleMeasurement() and readSingleMeasurementRaw() ---

class SingleMeasurementChannelTest : public MeasurementWorkflowFixture,
                                     public ::testing::WithParamInterface<Channel> {
};

TEST_P(SingleMeasurementChannelTest, ReadSingleMeasurementReturnsValidMeasurement) {
    const Measurement MEAS = client->readSingleMeasurement(GetParam());
    EXPECT_TRUE(MEAS.isValid());
    EXPECT_GE(MEAS.intp, 0);
    EXPECT_GE(static_cast<double>(MEAS.fracp), 0.0);
    EXPECT_LT(static_cast<double>(MEAS.fracp), 1.0);
    EXPECT_THAT(MEAS.toString(), MatchesRegex(MEASUREMENT_LINE_PATTERN));
}

TEST_P(SingleMeasurementChannelTest, ReadSingleMeasurementRawReturnsFormattedNonEmptyString) {
    const std::string RAW = client->readSingleMeasurementRaw(GetParam());
    EXPECT_FALSE(RAW.empty());
    EXPECT_THAT(RAW, MatchesRegex(MEASUREMENT_LINE_PATTERN));
}

// Channel 1 ticks every 10ms (100Hz, see FrameworkWorkflowFixture's VM), channel 2 every fixed 1s
// - both are cheap for a single read.
INSTANTIATE_TEST_SUITE_P(
    Channels,
    SingleMeasurementChannelTest,
    ::testing::Values(Channel::CH1, Channel::CH2)
);

// Each call to readSingleMeasurement() sends its own "e1" command and waits for the next tick, so
// two subsequent reads observe two distinct points on channel 1's 10ms grid - their values (and
// meas_num, which increments once per measurement on the VM) must not coincide.
TEST_F(MeasurementWorkflowFixture, SubsequentSingleMeasurementsDiffer) {
    const Measurement FIRST = client->readSingleMeasurement(Channel::CH1);
    const Measurement SECOND = client->readSingleMeasurement(Channel::CH1);
    EXPECT_NE(FIRST.toString(), SECOND.toString());
}


// --- Batch measurements: MeasContext options and their combinations ---

// Drains every measurement the processor thread produces into COLLECTED
static void collectAllMeasurements(MeasReader &reader, const MeasContext &/*meas_set*/,
                                   const Measurement &/*time_const*/,
                                   std::vector<Measurement> &collected) {
    while (const std::optional<Measurement> MEAS = reader.grabMeasFromProcessor(reader.for_monitor_q)) {
        collected.push_back(*MEAS);
    }
}

TEST_F(MeasurementWorkflowFixture, ReadBatchMeasurementsDefaultArgumentSucceeds) {
    // Exercises NPETComm::readBatchMeasurements()'s default MeasContext (5 measurements, channel
    // 1, no save, no monitor) - the possibility a caller passes nothing at all.
    EXPECT_NO_THROW(client->readBatchMeasurements());
    EXPECT_TRUE(client->isResponsive());
}

struct BatchMeasCountParams {
    std::string name;
    int num_of_meas;
};

class BatchMeasurementCountTest : public MeasurementWorkflowFixture,
                                  public ::testing::WithParamInterface<BatchMeasCountParams> {
};

TEST_P(BatchMeasurementCountTest, MonitorReceivesExactlyRequestedCount) {
    std::vector<Measurement> collected;
    const MeasContext CTX{
        .num_of_meas = GetParam().num_of_meas,
        .monitor_fn = [&collected](MeasReader &reader, const MeasContext &meas_set, const Measurement &time_const) {
            collectAllMeasurements(reader, meas_set, time_const, collected);
        },
        .save_path = std::nullopt,
        .channel = Channel::CH1,
    };
    client->readBatchMeasurements(CTX);
    ASSERT_EQ(collected.size(), static_cast<size_t>(GetParam().num_of_meas));
    for (const Measurement &meas: collected) {
        EXPECT_TRUE(meas.isValid());
        EXPECT_GE(static_cast<double>(meas.fracp), 0.0);
        EXPECT_LT(static_cast<double>(meas.fracp), 1.0);
    }
}

INSTANTIATE_TEST_SUITE_P(
    MeasCounts,
    BatchMeasurementCountTest,
    ::testing::Values(
        BatchMeasCountParams{"Single", 1},
        BatchMeasCountParams{"Default", 5},
        BatchMeasCountParams{"Many", 30}
    ),
    [](const ::testing::TestParamInfo<BatchMeasCountParams> &info) { return info.param.name; }
);


struct BatchMeasChannelParams {
    std::string name;
    Channel channel;
    int num_of_meas;
};

class BatchMeasurementChannelTest : public MeasurementWorkflowFixture,
                                    public ::testing::WithParamInterface<BatchMeasChannelParams> {
};

TEST_P(BatchMeasurementChannelTest, ReadsRequestedCountFromChannel) {
    std::vector<Measurement> collected;
    const MeasContext CTX{
        .num_of_meas = GetParam().num_of_meas,
        .monitor_fn = [&collected](MeasReader &reader, const MeasContext &meas_set, const Measurement &time_const) {
            collectAllMeasurements(reader, meas_set, time_const, collected);
        },
        .save_path = std::nullopt,
        .channel = GetParam().channel,
    };
    client->readBatchMeasurements(CTX);
    EXPECT_EQ(collected.size(), static_cast<size_t>(GetParam().num_of_meas));
}

INSTANTIATE_TEST_SUITE_P(
    Channels,
    BatchMeasurementChannelTest,
    ::testing::Values(
        // Channel 2 ticks once a second, so its count is kept small to keep the suite fast.
        BatchMeasChannelParams{"Channel1", Channel::CH1, 5},
        BatchMeasChannelParams{"Channel2", Channel::CH2, 1}
    ),
    [](const ::testing::TestParamInfo<BatchMeasChannelParams> &info) { return info.param.name; }
);


// Channel 2 ticks on a fixed 1s grid anchored to the VM's start time (see
// VirtualMachine::sendMeasurements()), so each successive measurement's whole-second part must be
// exactly one more than the last - unlike channel 1, whose 10ms tick period leaves room for the
// per-measurement timing jitter to occasionally carry a whole second early/late.
TEST_F(MeasurementWorkflowFixture, Channel2IntpIncrementsByOnePerMeasurement) {
    std::vector<Measurement> collected;
    const MeasContext CTX{
        .num_of_meas = 10,
        .monitor_fn = [&collected](MeasReader &reader, const MeasContext &meas_set, const Measurement &time_const) {
            collectAllMeasurements(reader, meas_set, time_const, collected);
        },
        .save_path = std::nullopt,
        .channel = Channel::CH2,
    };
    client->readBatchMeasurements(CTX);
    ASSERT_EQ(collected.size(), 10U);
    for (size_t i = 1; i < collected.size(); ++i) {
        EXPECT_EQ(collected.at(i).intp, collected.at(i - 1).intp + 1)
            << "Measurement " << i << " (intp=" << collected.at(i).intp
            << ") did not follow measurement " << (i - 1) << " (intp=" << collected.at(i - 1).intp << ") by 1 second";
    }
}


class BatchMeasurementSaveTest : public MeasurementWorkflowFixture {
protected:
    fs::path save_dir;

    void SetUp() override {
        MeasurementWorkflowFixture::SetUp();
        const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
        save_dir = fs::temp_directory_path() / "npet_workflow_test" / info->test_suite_name() / info->name();
        std::error_code ec;
        fs::remove_all(save_dir, ec);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(save_dir, ec);
    }

    // Every non-empty line written across all files under save_dir/FW_outputs.
    [[nodiscard]] std::vector<std::string> readSavedLines() const {
        std::vector<std::string> lines;
        const fs::path output_dir = save_dir / OUTPUT_DIR_NAME;
        std::error_code ec;
        if (!fs::exists(output_dir, ec)) {
            return lines;
        }
        for (const auto &entry: fs::directory_iterator(output_dir, ec)) {
            std::ifstream in(entry.path());
            std::string line;
            while (std::getline(in, line)) {
                if (!line.empty()) {
                    lines.push_back(line);
                }
            }
        }
        return lines;
    }

    [[nodiscard]] size_t savedFileCount() const {
        const fs::path OUTPUT_DIR = save_dir / OUTPUT_DIR_NAME;
        std::error_code ec;
        if (!fs::exists(OUTPUT_DIR, ec)) {
            return 0;
        }
        size_t count = 0;
        for (auto it = fs::directory_iterator(OUTPUT_DIR, ec); it != fs::directory_iterator(); ++it) {
            ++count;
        }
        return count;
    }
};

class BatchMeasurementSaveCountTest : public BatchMeasurementSaveTest,
                                      public ::testing::WithParamInterface<int> {
};

TEST_P(BatchMeasurementSaveCountTest, SaveTrueWritesOneLinePerMeasurement) {
    const MeasContext CTX{
        .num_of_meas = GetParam(), .monitor_fn = nullptr,
        .save_path = fs::path(outputFilePath(Channel::CH1, save_dir)), .channel = Channel::CH1
    };
    client->readBatchMeasurements(CTX);
    EXPECT_EQ(savedFileCount(), 1U);
    const std::vector<std::string> LINES = readSavedLines();
    ASSERT_EQ(LINES.size(), static_cast<size_t>(GetParam()));
    for (const std::string &line: LINES) {
        EXPECT_THAT(line, MatchesRegex(MEASUREMENT_LINE_PATTERN));
    }
}

INSTANTIATE_TEST_SUITE_P(
    MeasCounts,
    BatchMeasurementSaveCountTest,
    ::testing::Values(1, 3, 10)
);

TEST_F(BatchMeasurementSaveTest, SaveFalseWritesNoFile) {
    const MeasContext CTX{.num_of_meas = 3, .monitor_fn = nullptr, .save_path = std::nullopt, .channel = Channel::CH1};
    client->readBatchMeasurements(CTX);
    EXPECT_EQ(savedFileCount(), 0U);
}

TEST_F(BatchMeasurementSaveTest, SaveAndMonitorFnTogetherBothReceiveAllMeasurements) {
    std::vector<Measurement> collected;
    const MeasContext CTX{
        .num_of_meas = 3,
        .monitor_fn = [&collected](MeasReader &reader, const MeasContext &meas_set, const Measurement &time_const) {
            collectAllMeasurements(reader, meas_set, time_const, collected);
        },
        .save_path = fs::path(outputFilePath(Channel::CH1, save_dir)),
        .channel = Channel::CH1,
    };
    client->readBatchMeasurements(CTX);
    EXPECT_EQ(collected.size(), 3U);
    EXPECT_EQ(readSavedLines().size(), 3U);
}

// The keyboard watcher (see meas_reader.cpp's key_watcher thread) polls for Esc via _kbhit()/
// _getch(), which read from this process's own console input buffer. WriteConsoleInputW lets a
// test inject a synthetic key press into that same buffer, so the abort path can be exercised.
static void injectEscKeyPress() {
    INPUT_RECORD records[2]{};
    for (INPUT_RECORD &rec: records) {
        rec.EventType = KEY_EVENT;
        rec.Event.KeyEvent.wRepeatCount = 1;
        rec.Event.KeyEvent.wVirtualKeyCode = VK_ESCAPE;
        rec.Event.KeyEvent.wVirtualScanCode = static_cast<WORD>(MapVirtualKeyW(VK_ESCAPE, MAPVK_VK_TO_VSC));
        rec.Event.KeyEvent.uChar.AsciiChar = 27; // ESC
    }
    records[0].Event.KeyEvent.bKeyDown = TRUE;
    records[1].Event.KeyEvent.bKeyDown = FALSE;

    DWORD written = 0;
    ASSERT_TRUE(WriteConsoleInputW(GetStdHandle(STD_INPUT_HANDLE), records, 2, &written));
    ASSERT_EQ(written, 2U);
}

TEST_F(MeasurementWorkflowFixture, EscKeyPressInterruptsInfiniteOperation) {
    DWORD console_mode = 0;
    if (GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &console_mode) == 0) {
        GTEST_SKIP() << "No interactive console input attached to this process - injecting an Esc "
                        "key press requires a real console";
    }

    std::promise<void> first_measurement_seen;
    std::atomic<bool> promise_fulfilled{false};
    std::atomic<bool> aborted_flag{false};
    std::atomic<MeasReader *> reader_ptr{nullptr};
    std::vector<Measurement> collected;

    const MeasContext CTX{
        .num_of_meas = INFINITE_OPERATION,
        .monitor_fn = [&](MeasReader &reader, const MeasContext &, const Measurement &) {
            reader_ptr.store(&reader, std::memory_order_relaxed);
            while (const std::optional<Measurement> MEAS = reader.grabMeasFromProcessor(reader.for_monitor_q)) {
                collected.push_back(*MEAS);
                if (!promise_fulfilled.exchange(true)) {
                    first_measurement_seen.set_value();
                }
            }
            aborted_flag = reader.aborted.load();
        },
        .save_path = std::nullopt,
        .channel = Channel::CH1,
    };

    // readBatchMeasurements() blocks until the stream ends, so it needs its own thread while the
    // main thread waits for the stream to actually be flowing, then injects Esc mid-stream.
    std::future<void> read_done = std::async(std::launch::async, [&] {
        client->readBatchMeasurements(CTX);
    });

    // "infinite" truly never ends on its own, and read_done's destructor blocks until the async
    // task completes - so if Esc injection doesn't land (e.g. an unusual console/CI environment),
    // this test must still force the stream down itself, or it hangs the entire test binary rather
    // than just failing this one test.
    auto force_stop_if_still_running = [&] {
        if (read_done.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            if (MeasReader *reader = reader_ptr.load(std::memory_order_relaxed)) {
                reader->stop_sign.store(true, std::memory_order_relaxed);
            }
        }
    };

    if (first_measurement_seen.get_future().wait_for(std::chrono::seconds(2)) != std::future_status::ready) {
        force_stop_if_still_running();
        read_done.wait();
        FAIL() << "No measurement arrived before timing out - stream never started";
    }

    injectEscKeyPress();

    // key_watcher only polls every 500ms (see meas_reader.cpp), so give it - and the subsequent
    // thread shutdown - generous room before falling back to a direct stop.
    if (read_done.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
        force_stop_if_still_running();
        read_done.wait();
        ADD_FAILURE() << "Esc key press was not picked up by the keyboard watcher within 5s";
    }
    read_done.get(); // rethrows if readBatchMeasurements() threw

    EXPECT_TRUE(aborted_flag.load());
    // Channel 1 ticks every 10ms; well under a second of extra streaming before the watcher's next
    // poll notices Esc rules out "infinite" having simply run to its (effectively unbounded) end.
    EXPECT_LT(collected.size(), 1000U);
    EXPECT_TRUE(client->isResponsive());
}


// --- Average fraction: getAverageFraction() averages readSingleMeasurement()'s fracp over N reads ---

TEST_F(MeasurementWorkflowFixture, AverageFractionReturnsValueInUnitRange) {
    // Only AVER_NUM is given; CHANNEL_NUM defaults to CH2, which ticks once a second, so this is
    // kept to the minimum the implementation allows (assert(AVER_NUM > 2)) to keep the test fast.
    const std::optional<__float128> AVG = client->getAverageFraction(2);
    ASSERT_TRUE(AVG.has_value());
    EXPECT_GE(static_cast<double>(*AVG), 0.0);
    EXPECT_LT(static_cast<double>(*AVG), 1.0);
}


// Stand-in for NPET_comm_CLI.cpp's ProgressBar: any type with an update(int) method can be passed
// to getAverageFraction()'s optional progress-tracker template parameter.
struct ProgressRecorder {
    std::vector<int> calls;

    void update(const int PROGRESS) { calls.push_back(PROGRESS); }
};

TEST_F(MeasurementWorkflowFixture, AverageFractionReportsSequentialProgressToTracker) {
    constexpr int AVER_NUM = 6;
    ProgressRecorder tracker;
    const std::optional<__float128> AVG = client->getAverageFraction(AVER_NUM, Channel::CH1, &tracker);
    ASSERT_TRUE(AVG.has_value());
    ASSERT_EQ(tracker.calls.size(), static_cast<size_t>(AVER_NUM));
    for (int i = 0; i < AVER_NUM; ++i) {
        EXPECT_EQ(tracker.calls[i], i + 1);
    }
}

TEST_F(MeasurementWorkflowFixture, AverageFractionMatchesManuallyAveragedSingleMeasurements) {
    constexpr int AVER_NUM = 10;
    const std::optional<__float128> AVG = client->getAverageFraction(AVER_NUM, Channel::CH2);
    ASSERT_TRUE(AVG.has_value());

    __float128 reference_sum{};
    for (int i = 0; i < AVER_NUM; ++i) {
        reference_sum += client->readSingleMeasurement(Channel::CH2).fracp;
    }
    const auto REFERENCE_AVG = static_cast<double>(reference_sum / AVER_NUM);
    EXPECT_NEAR(static_cast<double>(*AVG), REFERENCE_AVG, 0.2);
}


// --- Clock time diff: getClockTimeDiff() combines readSingleMeasurement()'s intp with a clock reading ---

struct ClockTimeDiffParams {
    std::string name;
    int offset_from_measurement_intp;
};

class ClockTimeDiffExplicitSecondsTest : public MeasurementWorkflowFixture,
                                         public ::testing::WithParamInterface<ClockTimeDiffParams> {
};

// Channel 2 ticks on a fixed 1s grid, so a read's intp is always exactly one more than the last
// (see Channel2IntpIncrementsByOnePerMeasurement above). That lets BEFORE pin down exactly what
// intp the getClockTimeDiff() call below will observe, without racing the VM's own clock.
TEST_P(ClockTimeDiffExplicitSecondsTest, ReturnsClockSecondsMinusMeasurementIntp) {
    const Measurement BEFORE = client->readSingleMeasurement(Channel::CH2);
    const int EXPECTED_INTP = BEFORE.intp + 1;
    const int CLOCK_SECONDS = EXPECTED_INTP + GetParam().offset_from_measurement_intp;

    const int RESULT = client->getClockTimeDiff(Channel::CH2, CLOCK_SECONDS);
    EXPECT_EQ(RESULT, GetParam().offset_from_measurement_intp);
}

INSTANTIATE_TEST_SUITE_P(
    Offsets,
    ClockTimeDiffExplicitSecondsTest,
    ::testing::Values(
        ClockTimeDiffParams{"Zero", 0},
        ClockTimeDiffParams{"MeasurementAheadOfClock", 45},
        ClockTimeDiffParams{"MeasurementBehindClock", -45}
    ),
    [](const ::testing::TestParamInfo<ClockTimeDiffParams> &info) { return info.param.name; }
);

// Without an explicit clock_seconds, getClockTimeDiff() falls back to the system's local
// wall-clock time. REFERENCE, read right after, pins down the intp the call itself observed
// (again via channel 2's guaranteed +1-per-read grid), so RESULT can be checked against a
// same-formula local time reading, with a small tolerance for the (up to ~1s) VM round trip
// separating the two clock reads.
TEST_F(MeasurementWorkflowFixture, ClockTimeDiffWithoutClockSecondsUsesSystemLocalTime) {
    const int RESULT = client->getClockTimeDiff(Channel::CH2);
    const Measurement REFERENCE = client->readSingleMeasurement(Channel::CH2);
    const int MEAS_INTP_USED = REFERENCE.intp - 1;

    const std::time_t NOW = std::time(nullptr);
    std::tm local_time{};
    localtime_s(&local_time, &NOW);
    const int SECONDS_SINCE_MIDNIGHT = (local_time.tm_hour * 3600) + (local_time.tm_min * 60) + local_time.tm_sec;

    EXPECT_NEAR(RESULT, SECONDS_SINCE_MIDNIGHT - MEAS_INTP_USED, 2);
}
