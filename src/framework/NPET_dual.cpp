#include "NPET_dual.h"


static void setMeasuredDataFormatToBinary(NPETComm &npet) {
    SPDLOG_DEBUG("Setting measured data format to binary for NPET");
    if (!npet.setMeasuredDataFormat(0)) {
        SPDLOG_ERROR(DATA_FORMAT_ERR);
        throw std::runtime_error(std::string(DATA_FORMAT_ERR));
    }
}


void NPETDual::readBatchMeasurements(const DualMeasContext &meas_set) {
    SPDLOG_DEBUG("Reading batch measurements from Dual NPETs: {}", meas_set.toString());
    assert(meas_set.num_of_meas > 0);
    // Set the measured data format to binary
    // This program can only process the binary data format
    executeBoth([&] { setMeasuredDataFormatToBinary(start_); },
                [&] { setMeasuredDataFormatToBinary(stop_); });
}
