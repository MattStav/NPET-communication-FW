#include "NPET_dual.h"


///
/// Set measurement data format to binary on an NPET
/// @param npet NPETComm instance to set the measured data format
static void setMeasuredDataFormatToBinary(NPETComm &npet) {
    SPDLOG_DEBUG("Setting measured data format to binary for NPET");
    if (!npet.setMeasuredDataFormat(0)) {
        SPDLOG_ERROR(DATA_FORMAT_ERR);
        throw std::runtime_error(std::string(DATA_FORMAT_ERR));
    }
}

///
/// Start a measurement on NPET using the supplied measurement context.
/// @param npet NPETComm instance to start the measurement on
/// @param meas_set The measurement context containing the settings for the measurement
static void startMeasurement(NPETComm &npet, MeasContext const &meas_set) {
    [[maybe_unused]] MeasReader const SESSION(npet, meas_set);
}


void NPETDual::readBatchMeasurements(const DualMeasContext &meas_set) {
    SPDLOG_DEBUG("Reading batch measurements from Dual NPETs: {}", meas_set.toString());
    assert(meas_set.num_of_meas > 0);
    // Set the measured data format to binary
    // This program can only process the binary data format
    executeBoth([&] { setMeasuredDataFormatToBinary(start_); },
                [&] { setMeasuredDataFormatToBinary(stop_); });
    // Release the GIL to allow other threads to run while reading measurements
#ifdef PYBIND11_ENABLED
    pybind11::gil_scoped_release release;
#endif
    // Disable system sleep while this thread runs (Windows specific)
    if (SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED) == 0) {
        SPDLOG_ERROR(SLEEP_DISABLE_ERR);
        throw std::runtime_error(std::string(SLEEP_DISABLE_ERR));
    }
    // Call the measurement_reader on both NPETs
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
    executeBoth([&] { startMeasurement(start_, START_CTX); },
                [&] { startMeasurement(stop_, STOP_CTX); });
    // Re-enable system sleep after the critical function completes
    if (SetThreadExecutionState(ES_CONTINUOUS) == 0) {
        SPDLOG_ERROR(SLEEP_ENABLE_ERR);
        throw std::runtime_error(std::string(SLEEP_ENABLE_ERR));
    }
    SPDLOG_INFO("Batch measurements reading completed");
}
