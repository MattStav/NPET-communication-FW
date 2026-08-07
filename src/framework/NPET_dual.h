#ifndef NPET_COMM_FW_NPET_DUAL_H
#define NPET_COMM_FW_NPET_DUAL_H
#include "NPET_comm.h"


class NPETDual {
protected:
    NPETComm start_;
    NPETComm stop_;

public:
    // Functions to select NPET firmware version and save it into fw_version attribute
    void setFWVer(int NEW_FW_VERSION);

    // Set the baud rate on the NPET device
    [[nodiscard]] bool setBaudRate(int NEW_BAUD_RATE = 115200);

    // Read measurements from NPET
    void readBatchMeasurements(const MeasContext &meas_set = MeasContext{
        .num_of_meas = 5,
        .monitor_fn = nullptr,
        .save_dir = std::nullopt,
        .channel = Channel::CH1,
    });

    // Time correction constant handling on NPET
    [[nodiscard]] bool exportTimeConstant(const Measurement &constant);

    [[nodiscard]] bool exportTimeConstantRaw(const std::string &constant_raw = "");

    [[nodiscard]] bool clearTimeConstant();

    Measurement importTimeConstant();

    std::string importTimeConstantRaw();
};


#endif //NPET_COMM_FW_NPET_DUAL_H
