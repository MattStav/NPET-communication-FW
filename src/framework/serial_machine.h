#ifndef NPET_COMM_FW_SERIAL_MACHINE_H
#define NPET_COMM_FW_SERIAL_MACHINE_H
#include <chrono>
#include <optional>
#include <span>
#include <string>
#include <thread>
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
                                 std::chrono::milliseconds TIMEOUT);

public:
    // Constructor
    SerialMachine() = default;

    // TODO: Phase out the use of these getters
    [[nodiscard]] boost::asio::io_context &getIO() {
        return io_;
    }

    [[nodiscard]] boost::asio::serial_port &getPort() {
        return port_;
    }

    // Poll the io_context one handler at a time until PRED returns true.
    // @param RESTART Whether to restart the io_context before polling. Only valid when the io_context is
    //                 already stopped, i.e. there's no async operation left outstanding from a previous call
    //                 (the common case: PRED watches for the completion of an op posted right before this
    //                 call). Pass false when polling for an op that was posted further back and must stay
    //                 outstanding across multiple pollUntil calls (e.g. waiting on a clock in between).
    // @param THROTTLE Sleep duration between polls, to avoid busy-spinning a CPU core while waiting. Leave
    //                  at 0 on latency/throughput-sensitive paths (e.g. streaming data off the wire).
    template<typename Predicate>
    void pollUntil(Predicate PRED, const bool RESTART = true,
                   const std::chrono::milliseconds THROTTLE = std::chrono::milliseconds(0)) {
        if (RESTART) {
            io_.restart();
        }
        while (!PRED()) {
            io_.poll_one();
            if (THROTTLE.count() > 0) {
                std::this_thread::sleep_for(THROTTLE);
            }
        }
    }

    void openCommunication(int COM_PORT, int BAUD_RATE);

    void writeToSerial(const std::string &command);

    void cancelPendingOperation(bool BLOCK = true);

    [[nodiscard]] bool isOpen() const;

    void closeCommunication();

    void purgePort();

    int getBaudRate();

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
                                      std::chrono::milliseconds TIMEOUT = std::chrono::milliseconds(2000),
                                      std::size_t FIXED_BYTES = 1);

    void writeRawToSerial(std::span<const std::uint8_t> DATA);

    std::string readFromSerial(std::size_t MAX_BYTES = 128);

    void setBaudRateSerial(int NEW_BAUD_RATE);
};


#endif //NPET_COMM_FW_SERIAL_MACHINE_H
