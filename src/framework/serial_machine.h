#ifndef NPET_COMM_FW_SERIAL_MACHINE_H
#define NPET_COMM_FW_SERIAL_MACHINE_H
#include <chrono>
#include <csignal>
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
    // Watches for Ctrl+C, see armSigintShutdown()
    boost::asio::signal_set sigint_signals_{io_, SIGINT};

    ///
    /// Block until the pending read and timer operations have both completed, cancelling
    /// whichever one is still outstanding once the other finishes.
    /// @param timer Timer guarding the read operation
    /// @param read_result Set once the read operation completes
    /// @param timer_result Set once the timer fires or is cancelled
    void waitForReadOrTimeout(boost::asio::steady_timer &timer,
                              const std::optional<boost::system::error_code> &read_result,
                              const std::optional<boost::system::error_code> &timer_result);

    ///
    /// Translate a completed read/timer result pair into the appropriate exception, if any.
    /// @param read_result Result of the read operation
    /// @param timer_result Result of the timer operation
    /// @param TIMEOUT Timeout, used for the timeout error message
    static void throwOnReadError(const std::optional<boost::system::error_code> &read_result,
                                 const std::optional<boost::system::error_code> &timer_result,
                                 std::chrono::milliseconds TIMEOUT);

    ///
    /// Arm a handler that closes the serial connection cooperatively when SIGINT (Ctrl+C) is
    /// received, instead of relying on the OS's default handler, which force-kills threads and
    /// can deadlock the process if one of them was terminated mid-syscall inside a blocking
    /// serial port read. One-shot: call again after it fires if the port is reopened and the
    /// handler needs to be re-armed.
    void armSIGINTShutdown();

public:
    // Constructor
    SerialMachine() = default;


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

    ///
    /// Open serial communication on the specified COM port.
    /// @param COM_PORT COM port number to open communication on (e.g. 8 for COM8)
    /// @param BAUD_RATE Baud rate for the serial communication
    void openCommunication(int COM_PORT, int BAUD_RATE);

    ///
    /// Send a simple command over serial connection, does NOT await response.
    /// Makes sure the command is correctly terminated.
    /// @param command Command string to send to the device
    void writeToSerial(const std::string &command);

    ///
    /// Cancel any pending operation on the port. If BLOCK is true, wait until the
    /// cancellation has been processed by the io_context; otherwise only process
    /// whatever handlers are already ready without waiting.
    /// @param BLOCK Whether to block until the cancellation has been processed
    void cancelPendingOperation(bool BLOCK = true);

    ///
    /// @return Whether the serial port is currently open
    [[nodiscard]] bool isOpen() const;

    ///
    /// Close the serial port, if open.
    void closeCommunication();

    ///
    /// Purge the COM Port.
    /// Close the port and purge all handles.
    void purgePort();

    ///
    /// @return Current baud rate of the serial port
    int getBaudRate() const;

    ///
    /// Arm a one-shot, non-blocking read of exactly DATA.size() bytes from the serial port.
    /// Returns immediately; the actual read completes later on the io_context (see getIO()), so
    /// the caller must keep driving it (e.g. via pollUntil()) for COMPLETED to ever become true.
    /// @param DATA Buffer to fill; its size determines how many bytes are read
    /// @param ec Set to the read's resulting error code once it completes (default-constructed,
    /// i.e. falsy, on success). Must outlive the read, since it's captured by reference.
    /// @param completed Set to true once the read completes, successfully or otherwise. Must
    /// outlive the read, since it's captured by reference in the completion handler.
    void readExactAsync(std::span<std::uint8_t> DATA, boost::system::error_code &ec, bool &completed);

protected:
    /**
     * @brief Read mode for the read_with_timeout function.
     */
    enum class ReadMode : std::uint8_t {
        UNTIL_NEWLINE,
        FIXED_BYTES,
    };

    ///
    /// Send a command to the device and get the response as a string.
    /// @param command Command string to send to the device
    /// @return Device response string
    std::string exchangeComm(const std::string &command);

    ///
    /// Asynchronously read a response from the device, aborting if nothing arrives within the timeout.
    /// Does not send anything first; use this directly when a command has already been written,
    /// or a device response is expected unprompted (e.g. the virtual machine's device loop).
    /// @param MODE Mode to read the response, either until a newline character or a fixed number of bytes
    /// @param TIMEOUT Time to wait for a response before aborting the operation
    /// @param FIXED_BYTES Number of bytes to read if the mode is set to FixedBytes; unused otherwise
    /// @return Bytes read from the device
    std::vector<char> readWithTimeout(ReadMode MODE = ReadMode::UNTIL_NEWLINE,
                                      std::chrono::milliseconds TIMEOUT = std::chrono::milliseconds(2000),
                                      std::size_t FIXED_BYTES = 1);

    ///
    /// Send raw bytes over the serial connection as-is, without appending a line terminator.
    /// Use this for binary payloads (e.g. measurement packets), where appending "\r\n" would
    /// corrupt the data and desync the byte stream for the reader on the other end.
    /// @param DATA Raw bytes to send to the device
    void writeRawToSerial(std::span<const std::uint8_t> DATA);

    ///
    /// Read whatever is currently available on the serial port, up to max_bytes.
    /// This is a blocking call!
    /// @param MAX_BYTES Maximum number of bytes to read in this call
    /// @return Bytes read, converted to a string
    std::string readFromSerial(std::size_t MAX_BYTES = 128);

    ///
    /// Change the baud rate of the already-open serial port.
    /// @param NEW_BAUD_RATE Baud rate to switch to
    void setBaudRateSerial(int NEW_BAUD_RATE);

    ///
    /// Arm a one-shot, non-blocking listen for an incoming line starting with a specific byte.
    /// Returns immediately; the actual read completes later on the io_context (see getIO()), so the
    /// caller must keep driving it (e.g. via pollUntil()) for MATCHED to ever become true. Once a
    /// line has been read - or the read is cancelled, e.g. via getPort().cancel() - the listen is
    /// over and does not re-arm itself.
    /// @param EXPECTED_FIRST_BYTE Byte the received line must start with for MATCHED to be set
    /// @param matched Set to true if the received line starts with EXPECTED_FIRST_BYTE; left
    /// untouched otherwise. Must outlive the read, since it's captured by reference in the
    /// completion handler.
    void listenForCommand(char EXPECTED_FIRST_BYTE, bool &matched);
};


#endif //NPET_COMM_FW_SERIAL_MACHINE_H
