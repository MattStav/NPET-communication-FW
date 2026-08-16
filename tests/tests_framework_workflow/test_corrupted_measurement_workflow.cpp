#include "test_corrupted_measurement_fixture.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

#include "meas_func.h"

// Tests below exercise MeasReader's handling of a stream in which somme measurements get corrupted along the way.
// For testing purposes, this is facilitated by the VM deliberately corrupting every CORRUPT_EVERY-th measurement's checksum byte.
// The point is not that corruption is impossible to hit in
// practice, but that MeasReader::dataProcessor() must not propagate the resulting
// decodeMeasurementSet() exception - it should count the corrupted packet, discard it, and keep
// processing the rest of the stream (see the catch block in meas_reader.cpp).

namespace fs = std::filesystem;

// Within a single readBatchMeasurements() call of NUM_OF_MEAS measurements,
// corruption lands on exactly floor(NUM_OF_MEAS / CORRUPT_EVERY) of them
constexpr int NUM_OF_MEAS = 9;
constexpr int EXPECTED_CORRUPTED = NUM_OF_MEAS / CORRUPT_EVERY;
constexpr int EXPECTED_VALID = NUM_OF_MEAS - EXPECTED_CORRUPTED;

TEST_F(CorruptedMeasurementWorkflowFixture, ReadBatchMeasurementsDefaultArgumentSucceedsDespiteCorruption) {
    // Exercises NPETComm::readBatchMeasurements()'s default MeasContext (5 measurements) against a
    // stream that corrupts every 3rd one (tick 3) - the reader must swallow that one, not throw.
    EXPECT_NO_THROW(client->readBatchMeasurements());
    EXPECT_TRUE(client->isResponsive());
}

// Drains every measurement the processor thread produces into COLLECTED, and records the reader's
// final corrupted count once the stream has ended (reader stays alive - and its counters stable -
// for the whole call, since the monitor thread is joined before MeasReader::main() returns).
static void collectAndRecordCorrupted(MeasReader &reader, std::vector<Measurement> &collected,
                                      std::atomic<int> &corrupted_seen) {
    while (const std::optional<Measurement> MEAS = reader.grabMeasFromProcessor(reader.for_monitor_q)) {
        collected.push_back(*MEAS);
    }
    corrupted_seen.store(reader.corrupted.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

TEST_F(CorruptedMeasurementWorkflowFixture, DiscardsCorruptedMeasurementsAndKeepsProcessingTheRest) {
    std::vector<Measurement> collected;
    std::atomic corrupted_seen{-1};
    const MeasContext CTX{
        .num_of_meas = NUM_OF_MEAS,
        .monitor_fn = [&](MeasReader &reader, const MeasContext &, const Measurement &) {
            collectAndRecordCorrupted(reader, collected, corrupted_seen);
        },
        .save_path = std::nullopt,
        .channel = Channel::CH1,
    };
    EXPECT_NO_THROW(client->readBatchMeasurements(CTX));
    EXPECT_EQ(corrupted_seen.load(), EXPECTED_CORRUPTED);
    ASSERT_EQ(collected.size(), static_cast<size_t>(EXPECTED_VALID));
    for (const Measurement &meas: collected) {
        EXPECT_TRUE(meas.isValid());
        EXPECT_GE(static_cast<double>(meas.fracp), 0.0);
        EXPECT_LT(static_cast<double>(meas.fracp), 1.0);
    }
    EXPECT_TRUE(client->isResponsive());
}

// meas_num increments once per measurement the VM sends, corrupted or not (see
// VirtualMachine::sendMeasurements()), so the surviving, successfully-decoded measurements must
// still show gaps exactly where the discarded corrupted ones were - proving they were skipped
// rather than silently re-numbered.
TEST_F(CorruptedMeasurementWorkflowFixture, SurvivingMeasurementsSkipTheCorruptedTicks) {
    std::vector<Measurement> collected;
    std::atomic<int> corrupted_seen{-1};
    const MeasContext CTX{
        .num_of_meas = NUM_OF_MEAS,
        .monitor_fn = [&](MeasReader &reader, const MeasContext &, const Measurement &) {
            collectAndRecordCorrupted(reader, collected, corrupted_seen);
        },
        .save_path = std::nullopt,
        .channel = Channel::CH1,
    };
    client->readBatchMeasurements(CTX);
    ASSERT_EQ(collected.size(), static_cast<size_t>(EXPECTED_VALID));
    for (size_t i = 1; i < collected.size(); ++i) {
        EXPECT_GT(collected.at(i).meas_num, collected.at(i - 1).meas_num)
            << "meas_num must strictly increase across the surviving measurements";
    }
}


class CorruptedMeasurementSaveTest : public CorruptedMeasurementWorkflowFixture {
protected:
    fs::path save_dir;

    void SetUp() override {
        CorruptedMeasurementWorkflowFixture::SetUp();
        const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
        save_dir = fs::temp_directory_path() / "npet_corrupted_workflow_test" / info->test_suite_name() /
                   info->name();
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
};

// Corrupted packets are discarded before reaching for_saver_q_ (see MeasReader::dataProcessor's
// catch block), so the saved file must contain only the successfully-decoded measurements.
TEST_F(CorruptedMeasurementSaveTest, SaveWritesOnlyTheSuccessfullyDecodedMeasurements) {
    const MeasContext CTX{
        .num_of_meas = NUM_OF_MEAS,
        .monitor_fn = nullptr,
        .save_path = fs::path(outputFilePath(Channel::CH1, save_dir)),
        .channel = Channel::CH1,
    };
    client->readBatchMeasurements(CTX);
    EXPECT_EQ(readSavedLines().size(), static_cast<size_t>(EXPECTED_VALID));
}
