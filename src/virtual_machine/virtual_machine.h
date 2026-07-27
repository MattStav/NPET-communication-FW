#ifndef NPET_COMM_FW_VM_H
#define NPET_COMM_FW_VM_H
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

    std::string get_run_time() const;

    std::string get_response(const std::string &command);

    void change_baud_rate(int new_baud_rate);

    void send_measurements(const std::string &num_str, int sleep_ms);

public:
    explicit VirtualMachine(const int ch1_frequency) : ch1_frequency(ch1_frequency) {
    }

    void device_loop();
};


#endif //NPET_COMM_FW_VM_H
