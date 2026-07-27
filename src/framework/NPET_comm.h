#ifndef NPET_COMMUNICATOR_H
#define NPET_COMMUNICATOR_H
#include <spdlog/spdlog.h>

#include "meas_reader.h"
#include "meas_func.h"
#include "serial_machine.h"

static constexpr std::size_t MEASUREMENT_PACKET_SIZE = 13;
static constexpr int INFINITE_OPERATION = 9999;

class NPET_comm : public SerialMachine {
public:
    // Internal NPET firmware version
    int fw_version{};

    // Check if NPET is responsive
    [[nodiscard]] bool is_responsive(bool = false);

    // Functions to select NPET firmware version and save it into fw_version attribute
    void set_FW_ver(int new_fw_version);

    void detect_FW_ver();

    // Set the pulse generation frequency on the NPET device
    [[nodiscard]] bool set_frequency(int new_frequency = 100);

    // Generate pulses from the NPET device
    [[nodiscard]] bool generate_pulses(int num_of_pulses = 0);

    // Set the baud rate on the NPET device
    [[nodiscard]] bool set_baud_rate(int new_baud_rate = 115200);

    // Set the way NPET sends the measured data, 0 = binary, 1 = ASCII
    [[nodiscard]] bool set_measured_data_format(int format);

    // Read measurements from NPET
    void read_batch_measurements(const meas_context &meas_set = meas_context{
        .num_of_meas = 5,
        .monitor_fn = nullptr,
        .save = false,
        .channel = 1
    });

    measurement read_single_measurement(int channel);

    std::string read_single_measurement_raw(int channel);

    // Time correction constant handling on NPET
    [[nodiscard]] bool export_time_constant(const measurement &constant);

    [[nodiscard]] bool export_time_constant_raw(const std::string &constant_raw = "");

    [[nodiscard]] bool clear_time_constant();

    measurement import_time_constant();

    std::string import_time_constant_raw();

    std::string get_status();

    // Destructor
    ~NPET_comm() {
        SPDLOG_DEBUG("NPET comm destructor called, closing communication and resetting baud rate if possible");
        // Reset to default baud rate
        if (is_open()) {
            if (is_responsive()) (void) set_baud_rate(115200); // Ignore return value
            close_communication();
        }
    } // end of destructor
};


#endif //NPET_COMMUNICATOR_H
