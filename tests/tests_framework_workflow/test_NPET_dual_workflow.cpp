#include "test_dual_workflow_fixture.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Tests below exercise NPETDual's batch-reading surface over two live connections, each brought
// up by DualFrameworkWorkflowFixture::SetUpTestSuite() (see test_dual_workflow_fixture.h/.cpp).

namespace fs = std::filesystem;

TEST_F(DualFrameworkWorkflowFixture, BothConnectionsAreOpenAndResponsive) {
    ASSERT_TRUE(one_.ser.isOpen());
    ASSERT_TRUE(two_.ser.isOpen());
    EXPECT_TRUE(one_.isResponsive());
    EXPECT_TRUE(two_.isResponsive());
}

// switchStartStop() only relabels which live connection startComm()/stopComm() resolve to (see
// NPET_dual.h); both must still be the same open, responsive connections as one_/two_.
TEST_F(DualFrameworkWorkflowFixture, SwitchStartStopStillResolvesToLiveConnections) {
    switchStartStop();
    EXPECT_EQ(&startComm(), &two_);
    EXPECT_EQ(&stopComm(), &one_);
    EXPECT_TRUE(startComm().isResponsive());
    EXPECT_TRUE(stopComm().isResponsive());
}

TEST_F(DualFrameworkWorkflowFixture, ReadBatchMeasurementsDefaultArgumentSucceeds) {
    // Exercises NPETDual::readBatchMeasurements()'s default DualMeasContext (5 measurements,
    // channel 1 on both legs, no save) - the possibility a caller passes nothing at all.
    EXPECT_NO_THROW(readBatchMeasurements());
    EXPECT_TRUE(one_.isResponsive());
    EXPECT_TRUE(two_.isResponsive());
}

// executeBoth() (exercised here via readBatchMeasurements()) is documented to release both legs
// together off a shared latch rather than run them one after the other (see NPET_dual.h). Channel
// 1 ticks every 10ms (100Hz), so 100 measurements take ~1s; channel 2 ticks on a fixed 1s grid, so
// even a single measurement takes ~1s. Run sequentially that is >=2s; run concurrently, both legs
// finish around the same ~1s mark. The threshold below sits comfortably between the two.
TEST_F(DualFrameworkWorkflowFixture, ReadBatchMeasurementsRunsBothLegsConcurrently) {
    const DualMeasContext CTX{
        .num_of_meas = 100,
        .monitor_fn = nullptr,
        .save_dir = std::nullopt,
        .start_channel = Channel::CH1,
        .stop_channel = Channel::CH1,
    };
    const auto START_TIME = std::chrono::steady_clock::now();
    readBatchMeasurements(CTX);
    const auto ELAPSED = std::chrono::steady_clock::now() - START_TIME;
    EXPECT_LT(ELAPSED, std::chrono::milliseconds(1800))
        << "readBatchMeasurements() took " << std::chrono::duration_cast<std::chrono::milliseconds>(ELAPSED).count()
        << "ms - the start/stop legs do not appear to be running concurrently";
    EXPECT_TRUE(one_.isResponsive());
    EXPECT_TRUE(two_.isResponsive());
}

// DualMeasContext::monitor_fn is run on its own thread by readBatchMeasurements() (see
// NPET_dual.cpp), draining DualMeasReader::grabMeasurement() until both legs are done. Both legs
// read the same channel with the same num_of_meas, so every measurement produced by one leg's
// virtual machine has a same-meas_num counterpart from the other's (see
// VirtualMachine::deviceLoop() incrementing measurement_counter_ from 0) - all 5 pairs should
// come through matched.
TEST_F(DualFrameworkWorkflowFixture, MonitorFnReceivesAllCombinedMatchingMeasurements) {
    std::vector<DualMeasurement> received;
    std::optional<Measurement> seen_start_time_const;
    std::optional<Measurement> seen_stop_time_const;
    const DualMeasContext CTX{
        .num_of_meas = 5,
        .monitor_fn = [&](DualMeasReader &dual_reader, const DualMeasContext &,
                          const Measurement &start_time_const, const Measurement &stop_time_const) {
            seen_start_time_const = start_time_const;
            seen_stop_time_const = stop_time_const;
            while (const auto MEAS = dual_reader.grabMeasurement()) {
                received.push_back(*MEAS);
            }
        },
        .save_dir = std::nullopt,
        .start_channel = Channel::CH1,
        .stop_channel = Channel::CH1,
    };
    readBatchMeasurements(CTX);
    ASSERT_EQ(received.size(), 5U);
    for (const auto &pair: received) {
        EXPECT_EQ(pair.meas_start.meas_num, pair.meas_stop.meas_num)
            << "combined pair does not share a meas_num - matching logic paired the wrong measurements";
    }
    // Both legs' time constants must be known before the monitor starts running (see NPET_dual.cpp)
    ASSERT_TRUE(seen_start_time_const.has_value());
    ASSERT_TRUE(seen_stop_time_const.has_value());
    EXPECT_EQ(seen_start_time_const->meas_num, -1);
    EXPECT_EQ(seen_stop_time_const->meas_num, -1);
}

class DualBatchMeasurementSaveTest : public DualFrameworkWorkflowFixture {
protected:
    fs::path save_dir;

    void SetUp() override {
        DualFrameworkWorkflowFixture::SetUp();
        const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
        save_dir = fs::temp_directory_path() / "npet_dual_workflow_test" / info->test_suite_name() / info->name();
        std::error_code ec;
        fs::remove_all(save_dir, ec);
    }

    void TearDown() override {
        DualFrameworkWorkflowFixture::TearDown();
        std::error_code ec;
        fs::remove_all(save_dir, ec);
    }

    // Every non-empty line written across all files under save_dir/FW_outputs.
    [[nodiscard]] std::vector<std::string> readSavedLines() const {
        std::vector<std::string> lines;
        const fs::path OUTPUT_DIR = save_dir / OUTPUT_DIR_NAME;
        std::error_code ec;
        if (!fs::exists(OUTPUT_DIR, ec)) {
            return lines;
        }
        for (const auto &entry: fs::directory_iterator(OUTPUT_DIR, ec)) {
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

    // Filenames (not full paths) of every file saved under save_dir/FW_outputs.
    [[nodiscard]] std::vector<std::string> savedFileNames() const {
        std::vector<std::string> names;
        const fs::path OUTPUT_DIR = save_dir / OUTPUT_DIR_NAME;
        std::error_code ec;
        if (!fs::exists(OUTPUT_DIR, ec)) {
            return names;
        }
        for (const auto &entry: fs::directory_iterator(OUTPUT_DIR, ec)) {
            names.push_back(entry.path().filename().string());
        }
        return names;
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

// Output file names are prefixed "START_"/"STOP_" per leg (see NPET_dual.cpp), so the two legs'
// saved files never collide even when reading from different channels.
TEST_F(DualBatchMeasurementSaveTest, SaveWritesOneFilePerLegWhenChannelsDiffer) {
    const DualMeasContext CTX{
        .num_of_meas = 3,
        .monitor_fn = nullptr,
        .save_dir = save_dir,
        .start_channel = Channel::CH1,
        .stop_channel = Channel::CH2,
    };
    readBatchMeasurements(CTX);
    EXPECT_EQ(savedFileCount(), 2U);
    EXPECT_EQ(readSavedLines().size(), 6U); // 3 measurements from each of the two legs
}

// Same scenario as above, but with both legs reading the same channel - without the START/STOP
// prefix, both legs would resolve to the exact same "EPOCH1_<timestamp>.out" name (see
// outputFilePath() in meas_func.cpp) and collide; the prefix keeps them distinct regardless.
TEST_F(DualBatchMeasurementSaveTest, SaveWritesOneFilePerLegWhenChannelsMatch) {
    const DualMeasContext CTX{
        .num_of_meas = 3,
        .monitor_fn = nullptr,
        .save_dir = save_dir,
        .start_channel = Channel::CH1,
        .stop_channel = Channel::CH1,
    };
    readBatchMeasurements(CTX);
    EXPECT_EQ(savedFileCount(), 2U);
    EXPECT_EQ(readSavedLines().size(), 6U); // 3 measurements from each of the two legs
    const std::vector<std::string> NAMES = savedFileNames();
    EXPECT_EQ(std::ranges::count_if(NAMES, [](const std::string &n) { return n.starts_with("START_"); }), 1);
    EXPECT_EQ(std::ranges::count_if(NAMES, [](const std::string &n) { return n.starts_with("STOP_"); }), 1);
}

// readBatchMeasurements() must keep working end-to-end through startComm()/stopComm() once the
// designation has been swapped, still producing one correctly-prefixed file per leg.
TEST_F(DualBatchMeasurementSaveTest, ReadBatchMeasurementsSucceedsAfterSwitchStartStop) {
    switchStartStop();
    const DualMeasContext CTX{
        .num_of_meas = 3,
        .monitor_fn = nullptr,
        .save_dir = save_dir,
        .start_channel = Channel::CH1,
        .stop_channel = Channel::CH2,
    };
    EXPECT_NO_THROW(readBatchMeasurements(CTX));
    EXPECT_EQ(savedFileCount(), 2U);
    EXPECT_EQ(readSavedLines().size(), 6U); // 3 measurements from each of the two legs
}

TEST_F(DualBatchMeasurementSaveTest, NoSaveDirWritesNoFile) {
    const DualMeasContext CTX{
        .num_of_meas = 2,
        .monitor_fn = nullptr,
        .save_dir = std::nullopt,
        .start_channel = Channel::CH1,
        .stop_channel = Channel::CH2,
    };
    readBatchMeasurements(CTX);
    EXPECT_EQ(savedFileCount(), 0U);
}
