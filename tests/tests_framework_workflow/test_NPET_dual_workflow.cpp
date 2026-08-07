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
    ASSERT_TRUE(start_.isOpen());
    ASSERT_TRUE(stop_.isOpen());
    EXPECT_TRUE(start_.isResponsive());
    EXPECT_TRUE(stop_.isResponsive());
}

TEST_F(DualFrameworkWorkflowFixture, ReadBatchMeasurementsDefaultArgumentSucceeds) {
    // Exercises NPETDual::readBatchMeasurements()'s default DualMeasContext (5 measurements,
    // channel 1 on both legs, no save) - the possibility a caller passes nothing at all.
    EXPECT_NO_THROW(readBatchMeasurements());
    EXPECT_TRUE(start_.isResponsive());
    EXPECT_TRUE(stop_.isResponsive());
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
        .stop_channel = Channel::CH2,
    };
    const auto START_TIME = std::chrono::steady_clock::now();
    readBatchMeasurements(CTX);
    const auto ELAPSED = std::chrono::steady_clock::now() - START_TIME;
    EXPECT_LT(ELAPSED, std::chrono::milliseconds(1800))
        << "readBatchMeasurements() took " << std::chrono::duration_cast<std::chrono::milliseconds>(ELAPSED).count()
        << "ms - the start/stop legs do not appear to be running concurrently";
    EXPECT_TRUE(start_.isResponsive());
    EXPECT_TRUE(stop_.isResponsive());
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
