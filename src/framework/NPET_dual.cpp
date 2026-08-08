#include "NPET_dual.h"


void NPETDual::readBatchMeasurements(const DualMeasContext &meas_set) {
    SPDLOG_DEBUG("Reading batch measurements from Dual NPETs: {}", meas_set.toString());
    assert(meas_set.num_of_meas > 0);
    dual_reader_.reset();
    // This program can only process the binary data format
    executeBoth([&] { setMeasuredDataFormatToBinary(startComm()); },
                [&] { setMeasuredDataFormatToBinary(stopComm()); });
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
        .monitor_fn = [this](MeasReader &reader, const MeasContext &ctx, const Measurement &time_const) {
            dual_reader_.combine(true, reader, ctx, time_const);
        },
        .save_path = START_SAVE_PATH,
        .channel = meas_set.start_channel,
    };
    auto const STOP_CTX = MeasContext{
        .num_of_meas = meas_set.num_of_meas,
        .monitor_fn = [this](MeasReader &reader, const MeasContext &ctx, const Measurement &time_const) {
            dual_reader_.combine(false, reader, ctx, time_const);
        },
        .save_path = STOP_SAVE_PATH,
        .channel = meas_set.stop_channel,
    };
    // If the caller supplied a combined monitor, run it on its own thread draining dual_reader_
    // for as long as either leg is still producing measurements.
    std::jthread dual_monitor;
    if (meas_set.monitor_fn) {
        Measurement stop_time_const{};
        Measurement start_time_const{};
        executeBoth([&] { start_time_const = startComm().importTimeConstant(); },
                    [&] { stop_time_const = stopComm().importTimeConstant(); });
        dual_monitor = std::jthread(meas_set.monitor_fn, std::ref(dual_reader_), std::cref(meas_set),
                                    std::cref(start_time_const), std::cref(stop_time_const));
    }
    runWithSleepDisabled([&] {
        executeBoth([&] { startMeasurement(startComm(), START_CTX); },
                    [&] { startMeasurement(stopComm(), STOP_CTX); });
    });
    if (dual_monitor.joinable()) {
        dual_monitor.join();
    }
    SPDLOG_INFO("Batch measurements reading completed");
}
