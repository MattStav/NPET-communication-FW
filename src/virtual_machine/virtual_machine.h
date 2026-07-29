#ifndef NPET_COMM_FW_VM_H
#define NPET_COMM_FW_VM_H
#include <chrono>

#include "meas_func.h"
#include "serial_machine.h"


class VirtualMachine : public SerialMachine {
    // Frequency of measurement stream on channel 1
    int ch1_frequency{};
    // The time constant currently saved in NPET
    std::string time_const{};
    // The measurement counter
    uint8_t measurement_counter = 0;
    // Flag denoting whether NPET_FW valid measurement format is set
    bool correct_meas_format_set = false;
    // The time when vm was started
    const std::chrono::time_point<std::chrono::high_resolution_clock> start_time =
            std::chrono::high_resolution_clock::now();
    // Fixed per-launch timing offset (tens of us, in seconds), simulating a device's inherent, constant clock skew
    const __float128 timing_offset = random_offset();

    [[nodiscard]] static __float128 random_offset();

    [[nodiscard]] std::string get_run_time() const;

    std::string get_response(const std::string &command);

    void change_baud_rate(int new_baud_rate);

    void send_measurements(const std::string &num_str, std::chrono::microseconds period);

    void listen_for_stop_command(bool &stop_requested);

public:
    explicit VirtualMachine(const int ch1_frequency) : ch1_frequency(ch1_frequency) {
    }

    void device_loop();
};


#endif //NPET_COMM_FW_VM_H
