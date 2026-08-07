#include "serial_machine.h"

#include <spdlog/spdlog.h>

constexpr std::string_view COMM_TIMEOUT_ERR = "Communication timeout: Device did not respond within {}ms";


///
/// Open serial communication on the specified COM port.
/// @param COM_PORT COM port number to open communication on (e.g. 8 for COM8)
/// @param BAUD_RATE Baud rate for the serial communication
void SerialMachine::openCommunication(const int COM_PORT, const int BAUD_RATE) {
    SPDLOG_INFO("Opening communication on COM{} ...", COM_PORT);
    assert(COM_PORT > 0);
    const std::string PORT_NAME{std::to_string(COM_PORT)};
    // Construct the Windows-specific device path
    const std::string FULL_PORT_NAME = R"(\\.\COM)" + PORT_NAME;
    // Open a COM port communication
    port_ = boost::asio::serial_port(io_, FULL_PORT_NAME);
    port_.set_option(boost::asio::serial_port_base::baud_rate(BAUD_RATE));
    SPDLOG_DEBUG("COM{} opened with {} baud rate ", COM_PORT, BAUD_RATE);
    assert(port_.is_open());
    SPDLOG_INFO("Communication on COM{} opened successfully", COM_PORT);
} // end of openCommunication function


///
/// Block until the pending read and timer operations have both completed, cancelling
/// whichever one is still outstanding once the other finishes.
/// @param timer Timer guarding the read operation
/// @param read_result Set once the read operation completes
/// @param timer_result Set once the timer fires or is cancelled
void SerialMachine::waitForReadOrTimeout(boost::asio::steady_timer &timer,
                                         std::optional<boost::system::error_code> &read_result,
                                         std::optional<boost::system::error_code> &timer_result) {
    while (!read_result || !timer_result) {
        io_.run_one();
        if (read_result && !timer_result) {
            timer.cancel();
        } else if (timer_result && !read_result) {
            port_.cancel(); // This stops the pending async_read_until
        }
    }
}

///
/// Translate a completed read/timer result pair into the appropriate exception, if any.
/// @param read_result Result of the read operation
/// @param timer_result Result of the timer operation
/// @param TIMEOUT Timeout, used for the timeout error message
void SerialMachine::throwOnReadError(const std::optional<boost::system::error_code> &read_result,
                                     const std::optional<boost::system::error_code> &timer_result,
                                     const std::chrono::milliseconds TIMEOUT) {
    if (read_result && *read_result == boost::asio::error::operation_aborted) {
        // Distinguish a genuine timeout (the timer fired and cancelled the read) from
        // the read being cancelled for another reason.
        if (timer_result && !*timer_result) {
            SPDLOG_ERROR(COMM_TIMEOUT_ERR, TIMEOUT.count());
            throw CommTimeoutError(std::format(COMM_TIMEOUT_ERR, TIMEOUT.count()));
        }
        SPDLOG_DEBUG("Read operation was cancelled");
        throw OperationCancelledError("Read operation was cancelled");
    }
    if (!read_result || *read_result) {
        SPDLOG_ERROR("Read error: {}", read_result ? read_result->message() : "Unknown error");
        throw std::runtime_error("Read error: " + (read_result ? read_result->message() : "Unknown error"));
    }
}

///
/// Asynchronously read a response from the device, aborting if nothing arrives within the timeout.
/// Does not send anything first; use this directly when a command has already been written,
/// or a device response is expected unprompted (e.g. the virtual machine's device loop).
/// @param MODE Mode to read the response, either until a newline character or a fixed number of bytes
/// @param TIMEOUT Time to wait for a response before aborting the operation
/// @param FIXED_BYTES Number of bytes to read if the mode is set to FixedBytes; unused otherwise
/// @return Bytes read from the device
std::vector<char> SerialMachine::readWithTimeout(const ReadMode MODE,
                                                 const std::chrono::milliseconds TIMEOUT,
                                                 const std::size_t FIXED_BYTES) {
    SPDLOG_DEBUG("Reading with timeout, Mode: {}, Fixed bytes: {}, Timeout: {}ms",
                 MODE == ReadMode::UNTIL_NEWLINE ? "UntilNewline" : "FixedBytes", FIXED_BYTES, TIMEOUT.count());
    const auto RESPONSE_BUFFER = std::make_shared<boost::asio::streambuf>();
    std::optional<boost::system::error_code> timer_result;
    std::optional<boost::system::error_code> read_result;
    boost::asio::steady_timer timer(io_);
    std::size_t bytes_transferred = 0;
    std::vector<char> buffer;

    assert(port_.is_open());
    // Run the async read in a separate thread, with timeout
    timer.expires_after(TIMEOUT);
    timer.async_wait([&](const boost::system::error_code &ec) { timer_result = ec; });
    if (MODE == ReadMode::UNTIL_NEWLINE) {
        boost::asio::async_read_until(
            port_, *RESPONSE_BUFFER, "\n",
            [&](const boost::system::error_code &ec, const std::size_t BT) {
                read_result = ec;
                bytes_transferred = BT;
            }
        );
    } else if (MODE == ReadMode::FIXED_BYTES) {
        assert(FIXED_BYTES > 0);
        buffer.resize(FIXED_BYTES);
        boost::asio::async_read(
            port_,
            boost::asio::buffer(buffer),
            boost::asio::transfer_exactly(FIXED_BYTES),
            [&](const boost::system::error_code &ec, const std::size_t BT) {
                read_result = ec;
                bytes_transferred = BT;
            }
        );
    }
    // Block until ONLY this call's own operations (read + timer) have both completed.
    io_.restart();
    waitForReadOrTimeout(timer, read_result, timer_result);
    throwOnReadError(read_result, timer_result, TIMEOUT);
    // Convert the buffer to a vector of chars
    if (MODE == ReadMode::UNTIL_NEWLINE) {
        // Read the whole buffer; in some cases there can be more data then, bytes_transferred, e.g., constant import
        buffer = std::vector<char>{
            boost::asio::buffers_begin(RESPONSE_BUFFER->data()),
            boost::asio::buffers_end(RESPONSE_BUFFER->data())
        };
    } else if (MODE == ReadMode::FIXED_BYTES) {
        // async_read should have filled buffer; shrink to actual bytes if needed
        if (bytes_transferred < buffer.size()) {
            buffer.resize(bytes_transferred);
        }
    }
    SPDLOG_DEBUG("Received raw response from device: '{:?}'", std::string(buffer.begin(), buffer.end()));
    return buffer;
} // end of read_with_timeout function


///
/// Send a command to the device and get the response as a string.
/// @param command Command string to send to the device
/// @return Device response string
std::string SerialMachine::exchangeComm(const std::string &command) {
    assert(port_.is_open());
    assert(!command.empty());
    SPDLOG_DEBUG("Exchanging processed command with device: '{}'", command);
    writeToSerial(command);
    std::vector<char> buffer = readWithTimeout();
    // Convert the buffer to a string
    std::string response(buffer.begin(), buffer.end());
    // Remove trailing \n and \r
    while (!response.empty() && (response.back() == '\n' || response.back() == '\r')) {
        response.pop_back();
    }
    SPDLOG_DEBUG("Received processed response from device: '{:?}'", response);
    return response;
} // end of exchange_comm function


///
/// Send a simple command over serial connection, does NOT await response.
/// Makes sure the command is correctly terminated.
/// @param command Command string to send to the device
void SerialMachine::writeToSerial(const std::string &command) {
    assert(port_.is_open());
    SPDLOG_DEBUG("Writing to serial: {}", command);
    std::string full_command = command;
    if (!full_command.ends_with("\r\n")) {
        full_command += "\r\n";
    }
    boost::asio::write(port_, boost::asio::buffer(full_command));
}


///
/// Send raw bytes over the serial connection as-is, without appending a line terminator.
/// Use this for binary payloads (e.g. measurement packets), where appending "\r\n" would
/// corrupt the data and desync the byte stream for the reader on the other end.
/// @param DATA Raw bytes to send to the device
void SerialMachine::writeRawToSerial(const std::span<const std::uint8_t> DATA) {
    assert(port_.is_open());
    boost::asio::write(port_, boost::asio::buffer(DATA.data(), DATA.size()));
}


///
/// Read whatever is currently available on the serial port, up to max_bytes.
/// This is a blocking call!
/// @param MAX_BYTES Maximum number of bytes to read in this call
/// @return Bytes read, converted to a string
std::string SerialMachine::readFromSerial(const std::size_t MAX_BYTES) {
    assert(port_.is_open());
    std::vector<char> buffer(MAX_BYTES);
    const std::size_t BYTES_READ = port_.read_some(boost::asio::buffer(buffer));
    std::string response(buffer.data(), BYTES_READ);
    SPDLOG_DEBUG("Read from serial: '{:?}'", response);
    return response;
}


///
/// Purge the COM Port.
/// Close the port and purge all handles.
void SerialMachine::purgePort() {
    SPDLOG_DEBUG("Cancelling pending comms and purging all buffers ...");
    port_.cancel();
    PurgeComm(port_.native_handle(), PURGE_RXCLEAR | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_TXABORT);
}
