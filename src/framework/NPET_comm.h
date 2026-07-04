#ifndef NPET_COMMUNICATOR_H
#define NPET_COMMUNICATOR_H
#include <boost/asio.hpp>
#include <spdlog/spdlog.h>

#include "measurement_reader.h"
#include "meas_func.h"

// Number defining infinite operation
constexpr int INFINITE_OP = 9999;


class NPET_comm {
public:
    static constexpr std::size_t PACKET_SIZE = 13;

private:
    enum class ReadMode {
        UntilNewline,
        FixedBytes
    };

    std::vector<char> exchange_comm_raw(const std::string &command,
                                        ReadMode mode = ReadMode::UntilNewline,
                                        std::size_t fixed_bytes = PACKET_SIZE,
                                        int timeout = 2000);

    std::string exchange_comm(const std::string &command);

public:
    // io_context to manage the serial port's I/O operations
    boost::asio::io_context io;
    // Serial port object for communication
    boost::asio::serial_port port;
    // NPET firmware version
    int fw_version{};

    // Constructor
    NPET_comm() : port(io) {
    }

    // Open communication with NPET over serial COM port
    void open_communication(int com_port);

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
        if (this->port.is_open()) {
            if (this->is_responsive()) (void) set_baud_rate(115200); // Ignore return value
            port.close();
        }
    } // end of destructor
};


#endif //NPET_COMMUNICATOR_H
