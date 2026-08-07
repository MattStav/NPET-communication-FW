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
    // Call the measurement_reader on both NPETs, with Windows sleep disabled while it runs
    auto const START_CTX = MeasContext{
        .num_of_meas = meas_set.num_of_meas,
        .monitor_fn = nullptr, // TODO: Implement combination monitor
        .save_dir = meas_set.save_dir,
        .channel = meas_set.start_channel,
    };
    auto const STOP_CTX = MeasContext{
        .num_of_meas = meas_set.num_of_meas,
        .monitor_fn = nullptr, // TODO: Implement combination monitor
        .save_dir = meas_set.save_dir,
        .channel = meas_set.stop_channel,
    };
    runWithSleepDisabled([&] {
        executeBoth([&] { startMeasurement(start_, START_CTX); },
                    [&] { startMeasurement(stop_, STOP_CTX); });
    });
    SPDLOG_INFO("Batch measurements reading completed");
}
