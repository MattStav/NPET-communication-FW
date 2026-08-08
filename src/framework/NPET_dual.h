#ifndef NPET_COMM_FW_NPET_DUAL_H
#define NPET_COMM_FW_NPET_DUAL_H
#include <latch>
#include <thread>

#include "NPET_comm.h"
#include "meas_reader_dual.h"


class NPETDual {
protected:
    NPETComm one_;
    NPETComm two_;
    // Whether the START/STOP designation has been swapped relative to the physical
    bool designation_swapped_{false};

    // Currently designated START/STOP NPETComm, honoring switchStartStop().
    [[nodiscard]] NPETComm &startComm() { return designation_swapped_ ? two_ : one_; }

    [[nodiscard]] NPETComm &stopComm() { return designation_swapped_ ? one_ : two_; }

    // Combines the START/STOP legs' measurement streams into matched [meas_start, meas_stop]
    // pairs during readBatchMeasurements(); see meas_reader_dual.h
    DualMeasReader dual_reader_;

public:
    // Swap which underlying NPET is currently treated as START and which as STOP.
    // Does not touch the physical connections, only the logical designation.
    void switchStartStop() {
        designation_swapped_ = !designation_swapped_;
        SPDLOG_INFO("Switched NPET START/STOP designation; currently swapped: {}", designation_swapped_);
    }

    /// Predeclare a thread per callable, then release both at once off a common signal so
    /// they start in lockstep rather than one running ahead while the other is still spinning up.
    /// Blocks until both have finished.
    template<typename FuncOne, typename FuncTwo>
    void executeBoth(FuncOne &&func_one, FuncTwo &&func_two) {
        std::latch start_signal{1};
        std::jthread start_thread([&] {
            start_signal.wait();
            std::forward<FuncOne>(func_one)();
        });
        std::jthread stop_thread([&] {
            start_signal.wait();
            std::forward<FuncTwo>(func_two)();
        });
        start_signal.count_down();
    }

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
