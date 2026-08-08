#ifndef NPET_COMM_FW_VM_H
#define NPET_COMM_FW_VM_H
#include <chrono>

#include "meas_func.h"
#include "serial_machine.h"

struct VmConfig {
    int com_port{};
    int ch1_frequency{100};
    int corrupt_every{0};
};


class VirtualMachine : public SerialMachine {
    // Frequency of measurement stream on channel 1
    int ch1_frequency_{};
    // Corrupt every n-th measurement
    int corrupt_every_{};
    // The time constant currently saved in NPET
    std::string time_const_;
    // The measurement counter
    uint8_t measurement_counter_ = 0;
    // Flag denoting whether NPET_FW valid measurement format is set
    bool correct_meas_format_set_ = false;
    // The time when vm was started
    const std::chrono::time_point<std::chrono::high_resolution_clock> START_TIME =
            std::chrono::high_resolution_clock::now();
    // Fixed per-launch timing offset (tens of us, in seconds), simulating a device's inherent, constant clock skew
    const __float128 TIMING_OFFSET = randomOffset();

    [[nodiscard]] static __float128 randomOffset();

    [[nodiscard]] std::string getRunTime() const;

    std::string getResponse(const std::string &command);

    void changeBaudRate(int NEW_BAUD_RATE);

    void sendMeasurements(const std::string &num_str, std::chrono::microseconds PERIOD);

    void listenForStopCommand(bool &stop_requested);

public:
    explicit VirtualMachine(const VmConfig CONFIG) : ch1_frequency_(CONFIG.ch1_frequency),
                                                     corrupt_every_(CONFIG.corrupt_every) {
    }

    void deviceLoop();
};


#endif //NPET_COMM_FW_VM_H
