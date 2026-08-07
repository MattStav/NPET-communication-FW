#include "NPET_dual.h"


void NPETDual::readBatchMeasurements(const DualMeasContext &meas_set) {
    SPDLOG_DEBUG("Reading batch measurements from Dual NPETs: {}", meas_set.toString());
    assert(meas_set.num_of_meas > 0);
    // This program can only process the binary data format
    executeBoth([&] { setMeasuredDataFormatToBinary(start_); },
                [&] { setMeasuredDataFormatToBinary(stop_); });
    // Release the GIL to allow other threads to run while reading measurements
#ifdef PYBIND11_ENABLED
    pybind11::gil_scoped_release release;
#endif
    // Build each leg's save path prefixed with START/STOP so the two legs' output files
    // never collide, even when reading the same channel
    const std::optional<std::filesystem::path> START_SAVE_PATH = meas_set.save_dir
        ? std::optional{std::filesystem::path(outputFilePath(meas_set.start_channel, *meas_set.save_dir, "START"))}
        : std::nullopt;
    const std::optional<std::filesystem::path> STOP_SAVE_PATH = meas_set.save_dir
        ? std::optional{std::filesystem::path(outputFilePath(meas_set.stop_channel, *meas_set.save_dir, "STOP"))}
        : std::nullopt;
    // Call the measurement_reader on both NPETs, with Windows sleep disabled while it runs
    auto const START_CTX = MeasContext{
        .num_of_meas = meas_set.num_of_meas,
        .monitor_fn = nullptr, // TODO: Implement combination monitor and then add tests
        .save_path = START_SAVE_PATH,
        .channel = meas_set.start_channel,
    };
    auto const STOP_CTX = MeasContext{
        .num_of_meas = meas_set.num_of_meas,
        .monitor_fn = nullptr, // TODO: Implement combination monitor and then add tests
        .save_path = STOP_SAVE_PATH,
        .channel = meas_set.stop_channel,
    };
    runWithSleepDisabled([&] {
        executeBoth([&] { startMeasurement(start_, START_CTX); },
                    [&] { startMeasurement(stop_, STOP_CTX); });
    });
    SPDLOG_INFO("Batch measurements reading completed");
}
