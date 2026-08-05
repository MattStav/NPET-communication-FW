#ifndef NPET_COMM_FW_SERIAL_MACHINE_H
#define NPET_COMM_FW_SERIAL_MACHINE_H
#include <optional>
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
    boost::asio::io_context io_;
    // Serial port object for communication
    boost::asio::serial_port port_{io_};

    // Block until the pending read and timer operations have both completed,
    // cancelling whichever one is still outstanding once the other finishes.
    void waitForReadOrTimeout(boost::asio::steady_timer &timer,
                               std::optional<boost::system::error_code> &read_result,
                               std::optional<boost::system::error_code> &timer_result);

    // Translate a completed read/timer result pair into the appropriate exception, if any.
    static void throwOnReadError(const std::optional<boost::system::error_code> &read_result,
                                  const std::optional<boost::system::error_code> &timer_result,
                                  int TIMEOUT);

public:
    // Constructor
    SerialMachine() = default;

    // Open communication over serial COM port
    void openCommunication(int COM_PORT, int BAUD_RATE);

    [[nodiscard]] boost::asio::io_context &getIO() {
        return io_;
    }

    [[nodiscard]] boost::asio::serial_port &getPort() {
        return port_;
    }

    void writeToSerial(const std::string &command);

    [[nodiscard]] bool isOpen() const {
        return port_.is_open();
    }

    void closeCommunication() {
        if (port_.is_open()) {
            port_.close();
        }
    }

protected:
    /**
     * @brief Read mode for the read_with_timeout function.
     */
    enum class ReadMode : std::uint8_t {
        UNTIL_NEWLINE,
        FIXED_BYTES,
    };

    std::string exchangeComm(const std::string &command);

    std::vector<char> readWithTimeout(ReadMode MODE = ReadMode::UNTIL_NEWLINE,
                                        int TIMEOUT = 2000,
                                        std::size_t FIXED_BYTES = 1);

    void writeRawToSerial(std::span<const std::uint8_t> DATA);

    std::string readFromSerial(std::size_t MAX_BYTES = 128);
};


#endif //NPET_COMM_FW_SERIAL_MACHINE_H
