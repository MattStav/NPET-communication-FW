#include "serial_machine.h"

#include <spdlog/spdlog.h>

constexpr std::string_view COMM_TIMEOUT_ERR = "Communication timeout: Device did not respond within {}ms";


void Serial::openCommunication(const int COM_PORT, const int BAUD_RATE) {
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
    armSIGINTShutdown();
    SPDLOG_DEBUG("SIGINT shutdown armed");
    SPDLOG_INFO("Communication on COM{} opened successfully", COM_PORT);
} // end of openCommunication function


void Serial::waitForReadOrTimeout(boost::asio::steady_timer &timer,
                                         const std::optional<boost::system::error_code> &read_result,
                                         const std::optional<boost::system::error_code> &timer_result) {
    while (!read_result || !timer_result) {
        io_.run_one();
        if (read_result && !timer_result) {
            timer.cancel();
        } else if (timer_result && !read_result) {
            port_.cancel(); // This stops the pending async_read_until
        }
    }
}

void Serial::throwOnReadError(const std::optional<boost::system::error_code> &read_result,
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

std::vector<char> Serial::readWithTimeout(const ReadMode MODE,
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


std::string Serial::exchangeComm(const std::string &command) {
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


void Serial::writeToSerial(const std::string &command) {
    assert(port_.is_open());
    SPDLOG_DEBUG("Writing to serial: {}", command);
    std::string full_command = command;
    if (!full_command.ends_with("\r\n")) {
        full_command += "\r\n";
    }
    boost::asio::write(port_, boost::asio::buffer(full_command));
}


void Serial::writeRawToSerial(const std::span<const std::uint8_t> DATA) {
    assert(port_.is_open());
    boost::asio::write(port_, boost::asio::buffer(DATA.data(), DATA.size()));
}


std::string Serial::readFromSerial(const std::size_t MAX_BYTES) {
    assert(port_.is_open());
    std::vector<char> buffer(MAX_BYTES);
    const std::size_t BYTES_READ = port_.read_some(boost::asio::buffer(buffer));
    std::string response(buffer.data(), BYTES_READ);
    SPDLOG_DEBUG("Read from serial: '{:?}'", response);
    return response;
}


void Serial::cancelPendingOperation(const bool BLOCK) {
    port_.cancel();
    if (BLOCK) {
        io_.run();
    } else {
        io_.poll();
    }
}


bool Serial::isOpen() const {
    return port_.is_open();
}


void Serial::closeCommunication() {
    if (port_.is_open()) {
        port_.close();
    }
}


void Serial::purgePort() {
    SPDLOG_DEBUG("Cancelling pending comms and purging all buffers ...");
    port_.cancel();
    PurgeComm(port_.native_handle(), PURGE_RXCLEAR | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_TXABORT);
}


int Serial::getBaudRate() const {
    boost::asio::serial_port_base::baud_rate current_baud{};
    port_.get_option(current_baud);
    return static_cast<int>(current_baud.value());
}


void Serial::setBaudRate(const int NEW_BAUD_RATE) {
    port_.set_option(boost::asio::serial_port_base::baud_rate(NEW_BAUD_RATE));
    SPDLOG_INFO("Baud rate changed to {}", NEW_BAUD_RATE);
}


void Serial::listenForCommand(const char EXPECTED_FIRST_BYTE, bool &matched) {
    const auto LINE = std::make_shared<boost::asio::streambuf>();
    boost::asio::async_read_until(port_, *LINE, '\n',
                                  [&matched, LINE, EXPECTED_FIRST_BYTE](const boost::system::error_code &ec, std::size_t) {
                                      // ec set means the read was cancelled, e.g. once streaming ends; nothing to do
                                      if (const auto *first_byte = static_cast<const char *>(LINE->data().data());
                                          !ec && *first_byte == EXPECTED_FIRST_BYTE) {
                                          matched = true;
                                      }
                                  });
}


void Serial::readExactAsync(const std::span<std::uint8_t> DATA, boost::system::error_code &ec, bool &completed) {
    boost::asio::async_read(port_, boost::asio::buffer(DATA.data(), DATA.size()),
                            [&ec, &completed](const boost::system::error_code &error, std::size_t) {
                                ec = error;
                                completed = true;
                            });
}


void Serial::armSIGINTShutdown() {
    sigint_signals_.async_wait([this](const boost::system::error_code &ec, int) {
        if (ec) {
            return; // signal_set was cancelled/destroyed
        }
        SPDLOG_INFO("Shutdown requested, stopping serial communication ...");
        closeCommunication();
    });
}
