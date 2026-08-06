#include "test_workflow_fixture.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>
#include <string>
#include <vector>

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
        .save_dir = std::nullopt,
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
        .save_dir = std::nullopt,
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
        .save_dir = std::nullopt,
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
    const MeasContext CTX{.num_of_meas = GetParam(), .monitor_fn = nullptr, .save_dir = save_dir, .channel = 1};
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
    const MeasContext CTX{.num_of_meas = 3, .monitor_fn = nullptr, .save_dir = std::nullopt, .channel = 1};
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
        .save_dir = save_dir,
        .channel = 1,
    };
    client->readBatchMeasurements(CTX);
    EXPECT_EQ(collected.size(), 3U);
    EXPECT_EQ(readSavedLines().size(), 3U);
}

// TODO: test that Esc can interrupt