#include "virtual_machine.h"

#include <csignal>
#include <random>
#include <spdlog/spdlog.h>

#include "NPET_comm.h"


///
/// Generate a fixed, random timing offset for this VM instance, simulating a real device's
/// small, constant clock skew that stays put for as long as it's powered on.
/// @return A random offset expressed in seconds
__float128 VirtualMachine::random_offset() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution dist(1e-2, 1e-1);
    return dist(gen);
} // end of random_offset function


///
/// Get the current virtual machine run time
/// @return The current run time of the virtual machine formatted as hh:mm:ss.
std::string VirtualMachine::get_run_time() const {
    const std::chrono::time_point current_time = std::chrono::high_resolution_clock::now();
    const auto total_seconds = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
    const auto hours = total_seconds / 3600;
    const auto minutes = (total_seconds % 3600) / 60;
    const auto seconds = total_seconds % 60;
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << hours << ':'
            << std::setfill('0') << std::setw(2) << minutes << ':'
            << std::setfill('0') << std::setw(2) << seconds;
    return ss.str();
}

///
/// Process a received command and generate an appropriate response.
/// @param command Command string received
/// @return The response string to send back
std::string VirtualMachine::get_response(const std::string &command) {
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
        const int baud_rate = std::stoi(command.substr(1));
        SPDLOG_INFO("Changing baud rate to: {}", baud_rate);
        change_baud_rate(baud_rate);
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
        const int format = std::stoi(command.substr(1));
        SPDLOG_INFO("Measurement format set to {}", (format == 0 ? "binary" : "ASCII"));
        correct_meas_format_set = (format == 0);
        return command;
    }
    if (command.starts_with('e')) {
        SPDLOG_INFO("Reading measurements from channel {}", 1);
        send_measurements(command.substr(1), std::chrono::microseconds(1'000'000 / ch1_frequency));
        return "";
    }
    if (command.starts_with('h')) {
        SPDLOG_INFO("Reading measurements from channel {}", 2);
        send_measurements(command.substr(1), std::chrono::microseconds(1'000'000));
        return "";
    }
    if (command.starts_with('j')) {
        // Save time constant command
        const size_t line_end = command.find('\n');
        time_const = command.substr(1, line_end);
        SPDLOG_INFO("Received time constant: {:?}", time_const);
        return command;
    }
    if (command.starts_with('n')) {
        // Get time constant command
        const std::string tc = "n1\r\n" + time_const + "\r\n";
        SPDLOG_INFO("Sending time constant: {:?}", tc);
        return tc;
    }
    SPDLOG_ERROR("Undefined command for offline operation");
    return "";
} // end of get_response function


///
/// Change the baud rate of the serial port for the virtual device.
/// @param new_baud_rate New baud rate to set
void VirtualMachine::change_baud_rate(const int new_baud_rate) {
    try {
        get_port().set_option(boost::asio::serial_port_base::baud_rate(new_baud_rate));
        SPDLOG_INFO("Baud rate changed to {}", new_baud_rate);
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
void VirtualMachine::listen_for_stop_command(bool &stop_requested) {
    const auto line = std::make_shared<boost::asio::streambuf>();
    boost::asio::async_read_until(get_port(), *line, '\n',
                                  [&stop_requested, line](const boost::system::error_code &ec, std::size_t) {
                                      // ec set means the read was cancelled, e.g. once streaming ends; nothing to do
                                      if (const auto *first_byte = static_cast<const char *>(line->data().data());
                                          !ec && *first_byte == 'c')
                                          stop_requested = true;
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
/// @param period Fixed time between measurements (e.g. 1s, 250ms, 400us), anchored to start_time
void VirtualMachine::send_measurements(const std::string &num_str, const std::chrono::microseconds period) {
    if (!correct_meas_format_set) {
        SPDLOG_ERROR("Measurement format not set to binary, cannot send measurements");
        return;
    }
    std::random_device rd;
    std::mt19937 gen(rd());
    int number_of_measurements = std::stoi(num_str);
    int i{};
    // Emulate infinite operation, by generating as many measurements as possible
    if (number_of_measurements == INFINITE_OPERATION) number_of_measurements = std::numeric_limits<int>::max();
    SPDLOG_INFO("Number of measurements: {}", number_of_measurements);
    // Start listening for an incoming stop command without blocking the measurement stream below
    bool stop_requested = false;
    listen_for_stop_command(stop_requested);
    get_io().restart();
    // Align to the next tick of the start_time grid
    const auto ticks_elapsed = (std::chrono::high_resolution_clock::now() - start_time) / period;
    auto next_tick = start_time + (ticks_elapsed + 1) * period;
    while (true) {
        // Wait for the next fixed tick since start_time, while still polling for the stop command to arrive
        while (!stop_requested && std::chrono::high_resolution_clock::now() < next_tick) {
            get_io().poll_one();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (stop_requested) {
            SPDLOG_WARN("Stop command received, ending measurement sequence early");
            break;
        }
        // Increment the measurement counters
        measurement_counter++;
        i++;
        // Time the VM has been alive, trusted only down to the microsecond
        const auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(next_tick - start_time);
        const auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed_us);
        const auto sub_second_us = elapsed_us - elapsed_seconds; // whole microseconds within the current second
        const __float128 known_fracp = static_cast<__float128>(sub_second_us.count()) / 1'000'000;
        // Add this VM instance's fixed offset, plus hundred picosecond-level noise
        std::uniform_real_distribution jitter_distrib(0.0, 1e-10);
        __float128 fracp = known_fracp + timing_offset + static_cast<__float128>(jitter_distrib(gen));
        int seconds = static_cast<int>(elapsed_seconds.count());
        if (fracp >= 1) {
            // jitter pushed the fractional part past the next whole second
            fracp -= 1;
            seconds++;
        }
        const auto measurement_set = encode_measurement_set(
            measurement_counter, seconds, fracp, get_measurement_multiplier(3));
        write_raw_to_serial(measurement_set);
        if (i == number_of_measurements) {
            SPDLOG_INFO("Measurement stream completed: {} measurements sent", i);
            break;
        }
        next_tick += period;
    } // end of while loop
    // Stop listening for the stop command; harmless if it already completed
    get_port().cancel();
    get_io().poll();
    if (stop_requested) {
        SPDLOG_INFO("Stop command received mid-stream, ending measurement sequence early");
        write_to_serial("c1");
    }
} // end of send_measurements function


///
/// Mock NPET device function that simulates the behavior of a real NPET device.
/// Receives commands over a serial port and responds accordingly.
void VirtualMachine::device_loop() {
    // Handle Ctrl+C cooperatively: cancel the pending read instead of relying on the
    // OS's default handler, which force-kills threads and can deadlock the process if
    // one of them was terminated mid-syscall inside a blocking serial port read.
    boost::asio::signal_set signals(get_io(), SIGINT);
    signals.async_wait([this](const boost::system::error_code &ec, int) {
        if (ec) return; // signal_set was cancelled/destroyed
        SPDLOG_INFO("Shutdown requested, stopping virtual machine ...");
        if (is_open()) get_port().cancel();
    });

    while (true) {
        SPDLOG_INFO("Virtual NPET runtime [hh:mm:ss]: {}", get_run_time());
        SPDLOG_INFO("Waiting for data...");
        std::vector<char> buffer;
        try {
            buffer = read_with_timeout(ReadMode::UntilNewline, 10000);
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
        if (ret.empty()) continue;
        // Get a response to the request
        std::string response = get_response(ret);
        if (response.empty()) continue;
        SPDLOG_INFO("Writing response: {:?}", response);
        write_to_serial(response);
    } // end of while loop
    SPDLOG_INFO("Virtual machine shut down");
} // end of device_loop function
