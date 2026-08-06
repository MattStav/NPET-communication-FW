#include "test_workflow_fixture.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>
#include <string>
#include <vector>

#include "helper_func.h"
#include "meas_func.h"

// Tests below exercise NPETComm's measurement-reading surface (both the single-shot and the
// batch/streaming paths) over the live connection brought up by FrameworkWorkflowFixture

namespace fs = std::filesystem;
using ::testing::MatchesRegex;

// Matches Measurement::toString() / float128ToString()'s fixed 15-decimal-digit format, e.g. "0 1.234500000000000"
constexpr auto MEASUREMENT_LINE_PATTERN = R"(-?[0-9]+ -?[0-9]+\.[0-9]{15})";

// All measurement reads decode using npet_.fw_version's multiplier, which must match the
// multiplier the VM encoded with (VIRTUAL) or decoded values come out meaningless.
// Make sure the client properly detects the correct fw_version.
class MeasurementWorkflowFixture : public FrameworkWorkflowFixture {
protected:
    void SetUp() override {
        if (client) {
            client->detectFWVer();
            EXPECT_EQ(client->fw_version, FWVersion(FWVersion::VIRTUAL));
        }
    }
};


// --- Single measurement: both readSingleMeasurement() and readSingleMeasurementRaw() ---

class SingleMeasurementChannelTest : public MeasurementWorkflowFixture,
                                     public ::testing::WithParamInterface<int> {
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
    ::testing::Values(1, 2)
);

// Each call to readSingleMeasurement() sends its own "e1" command and waits for the next tick, so
// two subsequent reads observe two distinct points on channel 1's 10ms grid - their values (and
// meas_num, which increments once per measurement on the VM) must not coincide.
TEST_F(MeasurementWorkflowFixture, SubsequentSingleMeasurementsDiffer) {
    const Measurement FIRST = client->readSingleMeasurement(1);
    const Measurement SECOND = client->readSingleMeasurement(1);
    EXPECT_NE(FIRST.toString(), SECOND.toString());
}


// --- Batch measurements: MeasContext options and their combinations ---

// Drains every measurement the processor thread produces into COLLECTED, mirroring the
// must-fully-drain pattern used by readerCliSync() in meas_reader_CLI.cpp.
void collectAllMeasurements(MeasReader &reader, const MeasContext &/*meas_set*/, const Measurement &/*time_const*/,
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
        .save = false,
        .channel = 1,
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
    int channel;
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
        .save = false,
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
        BatchMeasChannelParams{"Channel1", 1, 5},
        BatchMeasChannelParams{"Channel2", 2, 1}
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
        .save = false,
        .channel = 2,
    };
    client->readBatchMeasurements(CTX);
    ASSERT_EQ(collected.size(), 10U);
    for (size_t i = 1; i < collected.size(); ++i) {
        EXPECT_EQ(collected[i].intp, collected[i - 1].intp + 1)
            << "Measurement " << i << " (intp=" << collected[i].intp
            << ") did not follow measurement " << (i - 1) << " (intp=" << collected[i - 1].intp << ") by 1 second";
    }
}


// Snapshots FW_outputs before each test, then can report and delete whatever readBatchMeasurements
// added, so save=true tests don't leave files behind in the user's real %APPDATA%\NPET directory
// (there is no way to redirect MeasReader::dataSaver() to a temp dir - it always targets USER_FILES).
class BatchMeasurementSaveTest : public MeasurementWorkflowFixture {
protected:
    fs::path output_dir = USER_FILES / OUTPUT_DIR_NAME;
    std::vector<fs::path> files_before;

    void SetUp() override {
        MeasurementWorkflowFixture::SetUp();
        std::error_code ec;
        if (fs::exists(output_dir, ec)) {
            for (const auto &entry: fs::directory_iterator(output_dir, ec)) {
                files_before.push_back(entry.path());
            }
        }
    }

    // Finds files added to output_dir since SetUp(), reads the first one's non-empty lines, then
    // deletes every newly added file - all before returning, so cleanup happens regardless of what
    // the caller's subsequent assertions do.
    struct SaveOutcome {
        size_t new_file_count{0};
        std::vector<std::string> lines;
    };

    [[nodiscard]] SaveOutcome captureAndCleanupNewFiles() const {
        SaveOutcome outcome{};
        std::error_code ec;
        if (!fs::exists(output_dir, ec)) {
            return outcome;
        }
        std::vector<fs::path> new_files;
        for (const auto &entry: fs::directory_iterator(output_dir, ec)) {
            if (std::ranges::find(files_before, entry.path()) == files_before.end()) {
                new_files.push_back(entry.path());
            }
        }
        outcome.new_file_count = new_files.size();
        if (!new_files.empty()) {
            std::ifstream in(new_files.front());
            std::string line;
            while (std::getline(in, line)) {
                if (!line.empty()) {
                    outcome.lines.push_back(line);
                }
            }
        }
        for (const auto &f: new_files) {
            fs::remove(f, ec);
        }
        return outcome;
    }
};

// TODO: Parametrize the number of meas
TEST_F(BatchMeasurementSaveTest, SaveTrueWritesOneLinePerMeasurement) {
    const MeasContext CTX{.num_of_meas = 3, .monitor_fn = nullptr, .save = true, .channel = 1};
    client->readBatchMeasurements(CTX);
    const SaveOutcome OUTCOME = captureAndCleanupNewFiles();
    ASSERT_EQ(OUTCOME.new_file_count, 1U);
    ASSERT_EQ(OUTCOME.lines.size(), 3U);
    for (const std::string &line: OUTCOME.lines) {
        EXPECT_THAT(line, MatchesRegex(MEASUREMENT_LINE_PATTERN));
    }
}

TEST_F(BatchMeasurementSaveTest, SaveFalseWritesNoFile) {
    const MeasContext CTX{.num_of_meas = 3, .monitor_fn = nullptr, .save = false, .channel = 1};
    client->readBatchMeasurements(CTX);
    EXPECT_EQ(captureAndCleanupNewFiles().new_file_count, 0U);
}

TEST_F(BatchMeasurementSaveTest, SaveAndMonitorFnTogetherBothReceiveAllMeasurements) {
    std::vector<Measurement> collected;
    const MeasContext CTX{
        .num_of_meas = 3,
        .monitor_fn = [&collected](MeasReader &reader, const MeasContext &meas_set, const Measurement &time_const) {
            collectAllMeasurements(reader, meas_set, time_const, collected);
        },
        .save = true,
        .channel = 1,
    };
    client->readBatchMeasurements(CTX);
    EXPECT_EQ(collected.size(), 3U);
    const SaveOutcome OUTCOME = captureAndCleanupNewFiles();
    ASSERT_EQ(OUTCOME.new_file_count, 1U);
    EXPECT_EQ(OUTCOME.lines.size(), 3U);
}

// TODO: test that Esc can interrupt