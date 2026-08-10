#ifndef NPET_COMM_FW_NPET_DUAL_H
#define NPET_COMM_FW_NPET_DUAL_H
#include <latch>
#include <thread>

#include "NPET_comm.h"
#include "meas_reader_dual.h"


class NPETDual {
    // Whether the START/STOP designation has been swapped relative to the physical
    bool designation_swapped_{false};

protected:
    NPETComm one_;
    NPETComm two_;

    ///
    /// Get the START NPET reference.
    /// @return The designated START NPET
    [[nodiscard]] NPETComm &start() { return designation_swapped_ ? two_ : one_; }

    ///
    /// Get the STOP NPET reference.
    /// @return The designated STOP NPET
    [[nodiscard]] NPETComm &stop() { return designation_swapped_ ? one_ : two_; }

    ///
    /// Swap which underlying NPET is currently treated as START and which as STOP.
    /// Does not touch the physical connections, only the logical designation.
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

    ///
    /// Start streaming measurements from the NPET.
    /// @param meas_set The Dual Measurement context.
    void readBatchMeasurements(const DualMeasContext &meas_set = DualMeasContext{
        .num_of_meas = 5,
        .monitor_fn = nullptr,
        .save_dir = std::nullopt,
        .start_channel = Channel::CH1,
        .stop_channel = Channel::CH1,
    });
};


#endif //NPET_COMM_FW_NPET_DUAL_H
