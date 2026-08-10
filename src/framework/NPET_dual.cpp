#include "NPET_dual.h"


void NPETDual::readBatchMeasurements(const DualMeasContext &meas_set) {
    SPDLOG_DEBUG("Reading batch measurements from Dual NPETs: {}", meas_set.toString());
    assert(meas_set.num_of_meas > 0);
    // Only needed to combine the two legs' streams for a caller-supplied monitor_fn, so skip it
    // entirely when there's nothing to drain it.
    std::optional<DualMeasReader> dual_reader;
    // This program can only process the binary data format
    SPDLOG_DEBUG("Setting measured data format to binary for both NPETs");
    executeBoth([&] { setMeasuredDataFormatToBinary(start()); },
                [&] { setMeasuredDataFormatToBinary(stop()); });
    // Release the GIL to allow other threads to run while reading measurements
#ifdef PYBIND11_ENABLED
    pybind11::gil_scoped_release release;
#endif
    // Build each leg's save path prefixed with START/STOP
    const std::optional<std::filesystem::path> START_SAVE_PATH = meas_set.save_dir
        ? std::optional{std::filesystem::path(outputFilePath(meas_set.start_channel, *meas_set.save_dir, "START"))}
        : std::nullopt;
    SPDLOG_DEBUG("Start save path: {}", START_SAVE_PATH ? START_SAVE_PATH->string() : "none");
    const std::optional<std::filesystem::path> STOP_SAVE_PATH = meas_set.save_dir
        ? std::optional{std::filesystem::path(outputFilePath(meas_set.stop_channel, *meas_set.save_dir, "STOP"))}
        : std::nullopt;
    SPDLOG_DEBUG("Stop save path: {}", STOP_SAVE_PATH ? STOP_SAVE_PATH->string() : "none");
    // Prepare the combination of each leg's monitoring into the provided monitor_fn
    std::function<void(MeasReader &, const MeasContext &, const Measurement &)> start_monitor_fn;
    std::function<void(MeasReader &, const MeasContext &, const Measurement &)> stop_monitor_fn;
    if (meas_set.monitor_fn) {
        // The two physical NPETs run independent free-running meas_num counters, so pull a single
        // measurement from each channel first to learn where each leg's counter currently stands;
        // that becomes the baseline DualMeasReader normalizes against when matching pairs.
        SPDLOG_DEBUG("Probing each leg's current meas_num before starting the batch ...");
        Measurement start_probe{};
        Measurement stop_probe{};
        executeBoth([&] { start_probe = start().readSingleMeasurement(meas_set.start_channel); },
                    [&] { stop_probe = stop().readSingleMeasurement(meas_set.stop_channel); });
        SPDLOG_DEBUG("Setting up dual monitoring functions ...");
        dual_reader.emplace(start_probe.meas_num, stop_probe.meas_num);
        start_monitor_fn = [&dual_reader](MeasReader &reader, const MeasContext &ctx, const Measurement &time_const) {
            dual_reader->combine(true, reader, ctx, time_const);
        };
        stop_monitor_fn = [&dual_reader](MeasReader &reader, const MeasContext &ctx, const Measurement &time_const) {
            dual_reader->combine(false, reader, ctx, time_const);
        };
    }
    // Call the measurement_reader on both NPETs
    auto const START_CTX = MeasContext{
        .num_of_meas = meas_set.num_of_meas,
        .monitor_fn = start_monitor_fn,
        .save_path = START_SAVE_PATH,
        .channel = meas_set.start_channel,
    };
    auto const STOP_CTX = MeasContext{
        .num_of_meas = meas_set.num_of_meas,
        .monitor_fn = stop_monitor_fn,
        .save_path = STOP_SAVE_PATH,
        .channel = meas_set.stop_channel,
    };
    // If the caller supplied a combined monitor, run it on its own thread draining dual_reader
    // for as long as either leg is still producing measurements.
    std::jthread dual_monitor;
    if (meas_set.monitor_fn) {
        SPDLOG_DEBUG("Starting dual monitoring thread ...");
        Measurement stop_time_const{};
        Measurement start_time_const{};
        executeBoth([&] { start_time_const = start().importTimeConstant(); },
                    [&] { stop_time_const = stop().importTimeConstant(); });
        dual_monitor = std::jthread(meas_set.monitor_fn, std::ref(*dual_reader), std::cref(meas_set),
                                    std::cref(start_time_const), std::cref(stop_time_const));
    }
    SPDLOG_DEBUG("Starting batch measurements reading ...");
    runWithSleepDisabled([&] {
        executeBoth([&] { startMeasurement(start(), START_CTX); },
                    [&] { startMeasurement(stop(), STOP_CTX); });
    });
    if (dual_monitor.joinable()) {
        dual_monitor.join();
    }
    SPDLOG_INFO("Batch measurements reading completed");
}


void NPETDual::switchStartStop() {
    designation_swapped_ = !designation_swapped_;
    SPDLOG_INFO("Switched NPET START/STOP designation; currently swapped: {}", designation_swapped_);
}

