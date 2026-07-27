#ifndef NPET_COMM_FW_SERIAL_MACHINE_H
#define NPET_COMM_FW_SERIAL_MACHINE_H
#include <string>
#include <vector>
#include <boost/asio.hpp>

class serial_machine {
    // io_context to manage the serial port's I/O operations
    boost::asio::io_context io;
    // Serial port object for communication
    boost::asio::serial_port port;

public:
    // Constructor
    serial_machine() : port(io) {
    }

    // Open communication over serial COM port
    void open_communication(int com_port, int baud_rate);

    [[nodiscard]] boost::asio::io_context &get_io() {
        return io;
    }

    [[nodiscard]] boost::asio::serial_port &get_port() {
        return port;
    }

    [[nodiscard]] bool is_open() const {
        return port.is_open();
    }

    void close_communication() {
        if (port.is_open()) {
            port.close();
        }
    }

    void write_to_serial(const std::string &command);

    std::string read_from_serial(std::size_t max_bytes = 128);

protected:
    enum class ReadMode {
        UntilNewline,
        FixedBytes
    };

    std::vector<char> exchange_comm_raw(const std::string &command,
                                        ReadMode mode = ReadMode::UntilNewline,
                                        std::size_t fixed_bytes = 1,
                                        int timeout = 2000);

    std::string exchange_comm(const std::string &command);
};


#endif //NPET_COMM_FW_SERIAL_MACHINE_H
