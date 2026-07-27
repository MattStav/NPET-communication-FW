#ifndef NPET_COMM_FW_SERIAL_MACHINE_H
#define NPET_COMM_FW_SERIAL_MACHINE_H
#include <span>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <stdexcept>

class CommTimeoutError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Thrown by read_with_timeout when the pending read is cancelled for a reason
// other than the timeout expiring (e.g. a requested shutdown).
class OperationCancelledError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class SerialMachine {
    // io_context to manage the serial port's I/O operations
    boost::asio::io_context io;
    // Serial port object for communication
    boost::asio::serial_port port;

public:
    // Constructor
    SerialMachine() : port(io) {
    }

    // Open communication over serial COM port
    void open_communication(int com_port, int baud_rate);

    [[nodiscard]] boost::asio::io_context &get_io() {
        return io;
    }

    [[nodiscard]] boost::asio::serial_port &get_port() {
        return port;
    }

    void write_to_serial(const std::string &command);

    [[nodiscard]] bool is_open() const {
        return port.is_open();
    }

    void close_communication() {
        if (port.is_open()) {
            port.close();
        }
    }

protected:
    enum class ReadMode {
        UntilNewline,
        FixedBytes
    };

    std::string exchange_comm(const std::string &command);

    std::vector<char> read_with_timeout(ReadMode mode = ReadMode::UntilNewline,
                                        int timeout = 2000,
                                        std::size_t fixed_bytes = 1);

    void write_raw_to_serial(std::span<const std::uint8_t> data);

    std::string read_from_serial(std::size_t max_bytes = 128);
};


#endif //NPET_COMM_FW_SERIAL_MACHINE_H
