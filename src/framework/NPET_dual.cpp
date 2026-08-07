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
}
