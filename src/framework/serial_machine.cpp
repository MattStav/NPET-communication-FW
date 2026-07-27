#include "serial_machine.h"

#include <spdlog/spdlog.h>

constexpr std::string_view COMM_TIMEOUT_ERR = "Communication timeout: Device did not respond within {}ms";


///
/// Open serial communication on the specified COM port.
/// @param com_port COM port number to open communication on (0-based index).
/// @param baud_rate Baud rate for the serial communication
void serial_machine::open_communication(const int com_port, const int baud_rate) {
    SPDLOG_INFO("Opening communication on COM{} ...", com_port + 1);
    assert(com_port > 0);
    // +1 needed for MS Windows correction
    const std::string port_name{std::to_string(com_port + 1)};
    // Construct the Windows-specific device path
    const std::string full_port_name = R"(\\.\COM)" + port_name;
    // Open a COM port communication
    port = boost::asio::serial_port(io, full_port_name);
    port.set_option(boost::asio::serial_port_base::baud_rate(baud_rate));
    SPDLOG_DEBUG("COM{} opened with {} baud rate ", com_port + 1, baud_rate);
    assert(port.is_open());
    SPDLOG_INFO("Communication on COM{} opened successfully", com_port + 1);
} // end of open_communication function


///
/// Send a command to the device and get the raw response buffer asynchronously.
/// @param command Command string to send to the device
/// @param mode Mode to read the response, either until a newline character or a fixed number of bytes
/// @param fixed_bytes Number of bytes to read if the mode is set to FixedBytes (default is 13, which is the typical response length for measurement data)
/// @param timeout Timeout in milliseconds to wait for a response before aborting the operation
/// @return Shared pointer to the response buffer
std::vector<char> serial_machine::exchange_comm_raw(const std::string &command,
                                                    const ReadMode mode,
                                                    const std::size_t fixed_bytes,
                                                    const int timeout) {
    SPDLOG_DEBUG("Exchanging raw command with device: '{}', Mode: {}, Fixed bytes: {}, Timeout: {}ms",
                 command, mode == ReadMode::UntilNewline ? "UntilNewline" : "FixedBytes", fixed_bytes, timeout);
    const auto response_buffer = std::make_shared<boost::asio::streambuf>();
    std::optional<boost::system::error_code> timer_result;
    std::optional<boost::system::error_code> read_result;
    boost::asio::steady_timer timer(io);
    std::size_t bytes_transferred = 0;
    std::vector<char> buffer;

    assert(port.is_open());
    assert(!command.empty());
    // Ensure the command ends with \r\n as expected by the device
    std::string full_command = command;
    if (!full_command.ends_with("\r\n")) full_command += "\r\n";
    boost::asio::write(port, boost::asio::buffer(full_command));
    // Run the async read in a separate thread, with timeout
    timer.expires_after(std::chrono::milliseconds(timeout));
    timer.async_wait([&](const boost::system::error_code &ec) { timer_result = ec; });
    if (mode == ReadMode::UntilNewline) {
        boost::asio::async_read_until(
            port, *response_buffer, "\n",
            [&](const boost::system::error_code &ec, const std::size_t bt) {
                read_result = ec;
                bytes_transferred = bt;
            }
        );
    } else {
        assert(fixed_bytes > 0);
        buffer.resize(fixed_bytes);
        boost::asio::async_read(
            port,
            boost::asio::buffer(buffer),
            boost::asio::transfer_exactly(fixed_bytes),
            [&](const boost::system::error_code &ec, const std::size_t bt) {
                read_result = ec;
                bytes_transferred = bt;
            }
        );
    }
    // Block until one of the operations completes
    io.restart();
    while (io.run_one()) {
        if (read_result) {
            timer.cancel();
        } else if (timer_result) {
            port.cancel(); // This stops the pending async_read_until
        }
    } // end of while loop
    if (read_result && *read_result == boost::asio::error::operation_aborted) {
        SPDLOG_ERROR(COMM_TIMEOUT_ERR, timeout);
        throw std::runtime_error(std::format(COMM_TIMEOUT_ERR, timeout));
    }
    if (!read_result || *read_result) {
        SPDLOG_ERROR("Read error: {}", read_result ? read_result->message() : "Unknown error");
        throw std::runtime_error("Read error: " + (read_result ? read_result->message() : "Unknown error"));
    }
    // Convert the buffer to a vector of chars
    if (mode == ReadMode::UntilNewline) {
        // Read the whole buffer; in some cases there can be more data then, bytes_transferred, e.g., constant import
        buffer = std::vector<char>{
            boost::asio::buffers_begin(response_buffer->data()),
            boost::asio::buffers_end(response_buffer->data())
        };
    } else {
        // async_read should have filled buffer; shrink to actual bytes if needed
        if (bytes_transferred < buffer.size()) buffer.resize(bytes_transferred);
    }
    SPDLOG_DEBUG("Received raw response from device: '{:?}'", std::string(buffer.begin(), buffer.end()));
    return buffer;
} // end of send_command_raw function


///
/// Send a command to the device and get the response as a string.
/// @param command Command string to send to the device
/// @return Device response string
std::string serial_machine::exchange_comm(const std::string &command) {
    SPDLOG_DEBUG("Exchanging processed command with device: '{}'", command);
    std::vector<char> buffer = exchange_comm_raw(command);
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
/// @param command Command string to send to the device
void serial_machine::write_to_serial(const std::string &command) {
    assert(port.is_open());
    SPDLOG_DEBUG("Writing to serial: {}", command);
    port.write_some(boost::asio::buffer(command));
}


///
/// Read whatever is currently available on the serial port, up to max_bytes.
/// @param max_bytes Maximum number of bytes to read in this call
/// @return Bytes read, converted to a string
std::string serial_machine::read_from_serial(const std::size_t max_bytes) {
    assert(port.is_open());
    std::vector<char> buffer(max_bytes);
    const std::size_t bytes_read = port.read_some(boost::asio::buffer(buffer));
    std::string response(buffer.begin(), buffer.begin() + bytes_read);
    SPDLOG_DEBUG("Read from serial: '{:?}'", response);
    return response;
}
