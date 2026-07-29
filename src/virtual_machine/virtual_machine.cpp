#include "virtual_machine.h"

#include <csignal>
#include <random>
#include <spdlog/spdlog.h>

#include "NPET_comm.h"


///
/// Generate a fixed, random timing offset for this VM instance, simulating a real device's
/// small, constant clock skew that stays put for as long as it's powered on.
/// @return A random offset expressed in seconds
__float128 VirtualMachine::randomOffset() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution dist(1e-2, 1e-1);
    return dist(gen);
} // end of random_offset function


///
/// Get the current virtual machine run time
/// @return The current run time of the virtual machine formatted as hh:mm:ss.
std::string VirtualMachine::getRunTime() const {
    const std::chrono::time_point CURRENT_TIME = std::chrono::high_resolution_clock::now();
    const auto TOTAL_SECONDS = std::chrono::duration_cast<std::chrono::seconds>(CURRENT_TIME - START_TIME).count();
    const auto HOURS = TOTAL_SECONDS / 3600;
    const auto MINUTES = (TOTAL_SECONDS % 3600) / 60;
    const auto SECONDS = TOTAL_SECONDS % 60;
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << HOURS << ':'
            << std::setfill('0') << std::setw(2) << MINUTES << ':'
            << std::setfill('0') << std::setw(2) << SECONDS;
    return ss.str();
}

///
/// Process a received command and generate an appropriate response.
/// @param command Command string received
/// @return The response string to send back
std::string VirtualMachine::getResponse(const std::string &command) {
    if (command.starts_with('c')) {
        // Communication check
        return "c0";
    }
    if (command.starts_with('?')) {
        // Firmware version request
        return "Firmware none - offline";
    }
    if (command.starts_with('w')) {
        // Change baud rate command
        const int BAUD_RATE = std::stoi(command.substr(1));
        SPDLOG_INFO("Changing baud rate to: {}", BAUD_RATE);
        changeBaudRate(BAUD_RATE);
        return command;
    }
    if (command.starts_with('p')) {
        // Fire pulses command
        SPDLOG_INFO("Firing Pulses: {}", command.substr(1));
        return command;
    }
    if (command.starts_with('k')) {
        // Change frequency command
        SPDLOG_INFO("Pulse firing frequency set to [Hz]: {}", command.substr(1));
        return command;
    }
    if (command.starts_with('a')) {
        // Change measured data format command
        const int FORMAT = std::stoi(command.substr(1));
        SPDLOG_INFO("Measurement format set to {}", (FORMAT == 0 ? "binary" : "ASCII"));
        correct_meas_format_set_ = (FORMAT == 0);
        return command;
    }
    if (command.starts_with('e')) {
        SPDLOG_INFO("Reading measurements from channel {}", 1);
        sendMeasurements(command.substr(1), std::chrono::microseconds(1'000'000 / ch1_frequency_));
        return "";
    }
    if (command.starts_with('h')) {
        SPDLOG_INFO("Reading measurements from channel {}", 2);
        sendMeasurements(command.substr(1), std::chrono::microseconds(1'000'000));
        return "";
    }
    if (command.starts_with('j')) {
        // Save time constant command
        const size_t LINE_END = command.find('\n');
        time_const_ = command.substr(1, LINE_END);
        SPDLOG_INFO("Received time constant: {:?}", time_const_);
        return command;
    }
    if (command.starts_with('n')) {
        // Get time constant command
        const std::string TC = "n1\r\n" + time_const_ + "\r\n";
        SPDLOG_INFO("Sending time constant: {:?}", TC);
        return TC;
    }
    SPDLOG_ERROR("Undefined command for offline operation");
    return "";
} // end of get_response function


///
/// Change the baud rate of the serial port for the virtual device.
/// @param NEW_BAUD_RATE New baud rate to set
void VirtualMachine::changeBaudRate(const int NEW_BAUD_RATE) {
    try {
        getPort().set_option(boost::asio::serial_port_base::baud_rate(NEW_BAUD_RATE));
        SPDLOG_INFO("Baud rate changed to {}", NEW_BAUD_RATE);
    } catch (const std::exception &e) {
        SPDLOG_ERROR("Couldn't change the baud rate: {}", e.what());
    }
} // end of change_baud_rate function


///
/// Arm a one-shot, non-blocking listen for a stop command ('c') on the serial port.
/// Returns immediately; the actual read completes later on the io_context (see get_io()), so the caller must
/// keep driving it (e.g. via poll_one()) for stop_requested to ever become true. Once a line has been read - or
/// the read is cancelled, e.g. via get_port().cancel() - the listen is over and does not re-arm itself.
/// @param stop_requested Set to true if the received line starts with 'c'; left untouched otherwise. Must
///                        outlive the read, since it's captured by reference in the completion handler.
void VirtualMachine::listenForStopCommand(bool &stop_requested) {
    const auto LINE = std::make_shared<boost::asio::streambuf>();
    boost::asio::async_read_until(getPort(), *LINE, '\n',
                                  [&stop_requested, LINE](const boost::system::error_code &ec, std::size_t) {
                                      // ec set means the read was cancelled, e.g. once streaming ends; nothing to do
                                      if (const auto *first_byte = static_cast<const char *>(LINE->data().data());
                                          !ec && *first_byte == 'c') {
                                          stop_requested = true;
                                      }
                                  });
}


///
/// Simulate sending measurement data over the serial port.
/// Measurements are fired on a fixed grid of times anchored to start_time (i.e. at start_time + k * period for
/// integer k). This mirrors a real NPET device, whose measurement ticks are driven by a free-running internal clock.
/// The first measurement is sent on the next upcoming grid tick.
/// Concurrently listens for an incoming stop command ('c'), which the client sends (e.g. via NPET_comm::is_responsive)
/// when the user cancels the measurement mid-stream (see meas_reader's Esc handling). If it arrives before all
/// measurements have been sent, streaming stops early and a "c1" response is sent back, mirroring how a real NPET
/// device acknowledges a stop request received while still streaming.
/// @param num_str Number of measurements to send as a string
/// @param PERIOD Fixed time between measurements (e.g. 1s, 250ms, 400us), anchored to start_time
void VirtualMachine::sendMeasurements(const std::string &num_str, const std::chrono::microseconds PERIOD) {
    if (!correct_meas_format_set_) {
        SPDLOG_ERROR("Measurement format not set to binary, cannot send measurements");
        return;
    }
    std::random_device rd;
    std::mt19937 gen(rd());
    int number_of_measurements = std::stoi(num_str);
    int i{};
    // Emulate infinite operation, by generating as many measurements as possible
    if (number_of_measurements == INFINITE_OPERATION) {
        number_of_measurements = std::numeric_limits<int>::max();
    }
    SPDLOG_INFO("Number of measurements: {}", number_of_measurements);
    // Start listening for an incoming stop command without blocking the measurement stream below
    bool stop_requested = false;
    listenForStopCommand(stop_requested);
    getIO().restart();
    // Align to the next tick of the start_time grid
    const auto TICKS_ELAPSED = (std::chrono::high_resolution_clock::now() - START_TIME) / PERIOD;
    auto next_tick = START_TIME + (TICKS_ELAPSED + 1) * PERIOD;
    while (true) {
        // Wait for the next fixed tick since start_time, while still polling for the stop command to arrive
        while (!stop_requested && std::chrono::high_resolution_clock::now() < next_tick) {
            getIO().poll_one();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (stop_requested) {
            SPDLOG_WARN("Stop command received, ending measurement sequence early");
            break;
        }
        // Increment the measurement counters
        measurement_counter_++;
        i++;
        // Time the VM has been alive, trusted only down to the microsecond
        const auto ELAPSED_US = std::chrono::duration_cast<std::chrono::microseconds>(next_tick - START_TIME);
        const auto ELAPSED_SECONDS = std::chrono::duration_cast<std::chrono::seconds>(ELAPSED_US);
        const auto SUB_SECOND_US = ELAPSED_US - ELAPSED_SECONDS; // whole microseconds within the current second
        const __float128 KNOWN_FRACP = static_cast<__float128>(SUB_SECOND_US.count()) / 1'000'000;
        // Add this VM instance's fixed offset, plus hundred picosecond-level noise
        std::uniform_real_distribution jitter_distrib(0.0, 1e-10);
        __float128 fracp = KNOWN_FRACP + TIMING_OFFSET + static_cast<__float128>(jitter_distrib(gen));
        int seconds = static_cast<int>(ELAPSED_SECONDS.count());
        if (fracp >= 1) {
            // jitter pushed the fractional part past the next whole second
            fracp -= 1;
            seconds++;
        }
        const Measurement MEASUREMENT{.meas_num = measurement_counter_, .intp = seconds, .fracp = fracp};
        const auto MEASUREMENT_SET = encodeMeasurementSet(MEASUREMENT, getMeasurementMultiplier(3));
        writeRawToSerial(MEASUREMENT_SET);
        if (i == number_of_measurements) {
            SPDLOG_INFO("Measurement stream completed: {} measurements sent", i);
            break;
        }
        next_tick += PERIOD;
    } // end of while loop
    // Stop listening for the stop command; harmless if it already completed
    getPort().cancel();
    getIO().poll();
    if (stop_requested) {
        SPDLOG_INFO("Stop command received mid-stream, ending measurement sequence early");
        writeToSerial("c1");
    }
} // end of send_measurements function


///
/// Mock NPET device function that simulates the behavior of a real NPET device.
/// Receives commands over a serial port and responds accordingly.
void VirtualMachine::deviceLoop() {
    // Handle Ctrl+C cooperatively: cancel the pending read instead of relying on the
    // OS's default handler, which force-kills threads and can deadlock the process if
    // one of them was terminated mid-syscall inside a blocking serial port read.
    boost::asio::signal_set signals(getIO(), SIGINT);
    signals.async_wait([this](const boost::system::error_code &ec, int) {
        if (ec) {
            return; // signal_set was cancelled/destroyed
        }
        SPDLOG_INFO("Shutdown requested, stopping virtual machine ...");
        if (isOpen()) {
            getPort().cancel();
        }
    });

    while (true) {
        SPDLOG_INFO("Virtual NPET runtime [hh:mm:ss]: {}", getRunTime());
        SPDLOG_INFO("Waiting for data...");
        std::vector<char> buffer;
        try {
            buffer = readWithTimeout(ReadMode::UNTIL_NEWLINE, 10000);
        } catch (const CommTimeoutError &e) {
            SPDLOG_DEBUG("No data received: {}", e.what());
            continue;
        } catch (const OperationCancelledError &) {
            break;
        }
        // Convert the buffer to a string
        std::string ret(buffer.begin(), buffer.end());
        // Remove trailing \n and \r
        while (!ret.empty() && (ret.back() == '\n' || ret.back() == '\r')) {
            ret.pop_back();
        }
        SPDLOG_INFO("Received raw command: '{}'", ret);
        if (ret.empty()) {
            continue;
        }
        // Get a response to the request
        std::string response = getResponse(ret);
        if (response.empty()) {
            continue;
        }
        SPDLOG_INFO("Writing response: {:?}", response);
        writeToSerial(response);
    } // end of while loop
    SPDLOG_INFO("Virtual machine shut down");
} // end of device_loop function
