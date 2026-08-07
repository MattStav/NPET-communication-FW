#ifndef NPET_COMM_FW_NPET_DUAL_H
#define NPET_COMM_FW_NPET_DUAL_H
#include "NPET_comm.h"


struct DualMeasContext {
    int num_of_meas{5};
    std::function<void(MeasReader &, const MeasContext &, const Measurement &)> monitor_fn = nullptr;
    // Directory saved measurements are written into; nullopt means don't save.
    std::optional<std::filesystem::path> save_dir{};
    Channel start_channel{Channel::CH1};
    Channel stop_channel{Channel::CH1};

    [[nodiscard]] std::string toString() const {
        return "dual_meas_context{num_of_meas: " + std::to_string(num_of_meas) +
               ", monitor_fn: " + (monitor_fn ? "set" : "null") +
               ", save_dir: " + (save_dir ? save_dir->string() : "null") +
               ", start_channel: " + std::to_string(static_cast<int>(start_channel)) +
               ", stop_channel: " + std::to_string(static_cast<int>(stop_channel)) + "}";
    }
};


class NPETDual {
protected:
    NPETComm start_;
    NPETComm stop_;

public:
    // Read measurements from NPET
    void readBatchMeasurements(const DualMeasContext &meas_set = DualMeasContext{
        .num_of_meas = 5,
        .monitor_fn = nullptr,
        .save_dir = std::nullopt,
        .start_channel = Channel::CH1,
        .stop_channel = Channel::CH1,
    });
};


#endif //NPET_COMM_FW_NPET_DUAL_H
