#include "NPET_dual.h"


///
/// Monitor function shared by both legs of a dual measurement. Runs as its own thread per leg
/// (spawned by MeasReader::main), draining that leg's for_monitor_q. Each measurement is matched
/// against the other leg's pending measurements by meas_num; once both halves of a pair have
/// arrived, the combined result is pushed onto for_dual_monitor_q for external consumption.
/// @param IS_START Whether this invocation is monitoring the START leg (true) or the STOP leg (false)
/// @param reader The MeasReader instance for this leg, owns this leg's own for_monitor_q
void NPETDual::combineMonitor(const bool IS_START, MeasReader &reader, const MeasContext & /*meas_set*/,
                              const Measurement & /*time_const*/) {
    while (true) {
        const std::optional<Measurement> MEAS = reader.grabMeasFromProcessor(reader.for_monitor_q);
        if (!MEAS) {
            break;
        }
        std::scoped_lock const LOCK(combine_mtx_);
        auto &other_pending = IS_START ? pending_stop_ : pending_start_;
        if (const auto IT = other_pending.find(MEAS->meas_num); IT != other_pending.end()) {
            for_dual_monitor_q.push(IS_START
                ? DualMeasurement{.meas_start = *MEAS, .meas_stop = IT->second}
                : DualMeasurement{.meas_start = IT->second, .meas_stop = *MEAS});
            other_pending.erase(IT);
        } else {
            auto &own_pending = IS_START ? pending_start_ : pending_stop_;
            own_pending.emplace(MEAS->meas_num, *MEAS);
        }
    } // end of while loop
} // end of combineMonitor function


std::optional<DualMeasurement> NPETDual::grabMeasFromDualMonitor() {
    std::scoped_lock const LOCK(combine_mtx_);
    if (for_dual_monitor_q.empty()) {
        return std::nullopt;
    }
    DualMeasurement ret = for_dual_monitor_q.front();
    for_dual_monitor_q.pop();
    return ret;
}


void NPETDual::readBatchMeasurements(const DualMeasContext &meas_set) {
    SPDLOG_DEBUG("Reading batch measurements from Dual NPETs: {}", meas_set.toString());
    assert(meas_set.num_of_meas > 0);
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
            combineMonitor(true, reader, ctx, time_const);
        },
        .save_path = START_SAVE_PATH,
        .channel = meas_set.start_channel,
    };
    auto const STOP_CTX = MeasContext{
        .num_of_meas = meas_set.num_of_meas,
        .monitor_fn = [this](MeasReader &reader, const MeasContext &ctx, const Measurement &time_const) {
            combineMonitor(false, reader, ctx, time_const);
        },
        .save_path = STOP_SAVE_PATH,
        .channel = meas_set.stop_channel,
    };
    runWithSleepDisabled([&] {
        executeBoth([&] { startMeasurement(startComm(), START_CTX); },
                    [&] { startMeasurement(stopComm(), STOP_CTX); });
    });
    SPDLOG_INFO("Batch measurements reading completed");
}
