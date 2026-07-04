#define WIN32_LEAN_AND_MEAN  // Prevent windows.h from including WinSock.h
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <boost/asio.hpp>
#include <random>    // For C++11 random number generation
#include "virtual_machine.h"

#include <rang.hpp>

#include "cli.h"
#include "meas_func.h"
#include "NPET_comm.h"

using namespace boost::asio;

///
/// Change the baud rate of the serial port for the mock device.
/// @param port Pointer to the serial port object
/// @param baud_rate New baud rate to set
void change_baud_rate(serial_port &port, const int baud_rate) {
    try {
        port.set_option(serial_port_base::baud_rate(baud_rate));
        cli::show_int("Baud rate changed to", baud_rate);
    } catch (const std::exception &e) {
        cli::err("Couldn't change the baud rate: " + std::string(e.what()));
    }
} // end of change_baud_rate function


///
/// Simulate sending measurement data over the serial port.
/// @param port Reference to the serial port object
/// @param num_str Number of measurements to send
/// @param measurement_counter Reference to the measurement counter to increment with each sent measurement
/// @param sleep_ms Delay between measurements in milliseconds
void send_measurements(serial_port &port, const std::string &num_str, uint8_t &measurement_counter, const int sleep_ms) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution distrib(0, 255);
    std::atomic stop_requested{false};
    std::atomic watcher_done{false};
    std::mutex write_mtx; // guard writes if both threads may write

    // RX watcher thread: waits for commands and stops on 'c'
    std::thread watcher([&] {
        boost::system::error_code ec;
        std::string rx_accum;
        char buf[256];

        while (!stop_requested.load(std::memory_order_relaxed)) {
            const std::size_t n = port.read_some(buffer(buf), ec); // blocking
            if (ec) {
                // If stream is ending and port gets canceled/closed, just exit
                break;
            }
            rx_accum.append(buf, buf + n);
            // parse complete lines
            for (;;) {
                auto pos = rx_accum.find("\r\n");
                if (pos == std::string::npos) break;
                std::string cmd = rx_accum.substr(0, pos);
                rx_accum.erase(0, pos + 2);
                if (!cmd.empty() && cmd[0] == 'c') {
                    stop_requested.store(true, std::memory_order_relaxed);
                    // respond once from watcher (optional: main thread can do it instead)
                    {
                        std::lock_guard lk(write_mtx);
                        boost::asio::write(port, buffer("c1\r\n"), ec);
                    }
                    break;
                }
            }
        }
        watcher_done.store(true, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        port.cancel(); // unblock watcher read_some if needed
        PurgeComm(port.native_handle(),PURGE_RXCLEAR | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_TXABORT);
    });

    int number_of_measurements = std::stoi(num_str);
    if (number_of_measurements == INFINITE_OP) number_of_measurements = std::numeric_limits<int>::max();
    cli::show_int("Number of measurements", number_of_measurements);
    for (int i = 0; i < number_of_measurements; ++i) {
        if (stop_requested.load(std::memory_order_relaxed)) {
            break;
        };
        // Increment the measurement counter
        measurement_counter++;
        // Construct the measurement
        std::array<uint8_t, 13> measurement{};
        measurement[0] = 1; // Start byte 1
        measurement[1] = 11; // Start byte 2
        measurement[2] = measurement_counter; // Measurement number
        measurement[3] = 241;
        measurement[4] = 10;
        measurement[5] = 10;
        measurement[6] = 145;
        measurement[7] = 145;
        measurement[8] = 159;
        measurement[9] = 143;
        measurement[10] = distrib(gen); // Random byte to simulate measurement data variability
        measurement[11] = 14;
        measurement[12] = xor_checksum(measurement);
        boost::system::error_code ec;
        {
            std::lock_guard lk(write_mtx);
            boost::asio::write(port, buffer(measurement, measurement.size()), ec);
        }
        if (ec) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    } // end of for loop
    // shutdown watcher
    stop_requested.store(true, std::memory_order_relaxed);
    if (watcher.joinable()) watcher.join();
} // end of send_measurements function


///
/// Process a received command and generate an appropriate response.
/// @param port Pointer to the serial port object
/// @param command Command string received
/// @param time_const Time correction constant storage
/// @param measurement_counter Storage for the last measurement number id
/// @param data_frequency Frequency of data measurement
/// @return The response string to send back
std::string get_response(
    serial_port &port,
    const std::string &command,
    std::string &time_const,
    uint8_t &measurement_counter,
    const int data_frequency
) {
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
        change_baud_rate(port, baud_rate);
        return command;
    }
    if (command.starts_with('p')) {
        // Fire pulses command
        cli::show_str("Firing Pulses", command.substr(1));
        return command;
    }
    if (command.starts_with('k')) {
        // Change frequency command
        cli::show_str("Frequency set to [Hz]", command.substr(1));
        return command;
    }
    if (command.starts_with('a')) {
        // Change measured data format command
        const int format = std::stoi(command.substr(1));
        cli::show_str("Format set to", format == 0 ? "binary" : "ASCII");
        return command;
    }
    if (command.starts_with('e')) {
        cli::show_int("Reading measurements from channel", 1);
        send_measurements(
            port,
            command.substr(1),
            measurement_counter,
            1000 / data_frequency
        );
        return "";
    }
    if (command.starts_with('h')) {
        cli::show_int("Reading measurements from channel", 2);
        send_measurements(
            port,
            command.substr(1),
            measurement_counter,
            1000
        );
        return "";
    }
    if (command.starts_with('j')) {
        // Save time constant command
        const size_t line_end = command.find('\n');
        time_const = command.substr(1, line_end);
        cli::show_str("Received time constant", time_const);
        return command;
    }
    if (command.starts_with('n')) {
        // Get time constant command
        const std::string ret = "n1\r\n" + time_const + "\r\n";
        cli::show_str("Sending time constant", ret);
        return ret;
    }
    cli::echo("Undefined command for offline operation", fg::yellow);
    return "";
} // end of get_response function

///
/// Mock NPET device function that simulates the behavior of a real NPET device.
/// Receives commands over a serial port and responds accordingly.
/// @param port Reference to the serial port object
/// @param data_frequency Data measurement frequency
void device_loop(serial_port &port, const int data_frequency) {
    char raw_data[100]{};
    // Time correction constant storage
    std::string time_const{};
    uint8_t measurement_counter = 0;
    boost::system::error_code ec;

    const std::chrono::time_point start_time = std::chrono::high_resolution_clock::now();
    // Run the device loop infinitely
    while (true) {
        std::chrono::time_point current_time = std::chrono::high_resolution_clock::now();
        // Convert the duration directly to a double-based minute duration
        std::chrono::duration<double, std::ratio<60> > duration = current_time - start_time;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << duration.count();
        cli::echo("----------------------------------------", fg::gray);
        cli::show_str("Offline NPET runtime [min]", ss.str());
        cli::echo("Waiting for data...", fg::gray);
        // Read data
        // This is a blocking call !!!
        const size_t bytesRead = port.read_some(buffer(raw_data, sizeof(raw_data) - 1), ec);
        // Error handling
        if (ec) {
            cli::err("Error reading from COM port: " + ec.message());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (cli::confirm("Do you wish to reset the virtual device?")) {
                cli::echo("Resetting device...", fg::yellow);
                continue; // Restart the loop
            }
            cli::echo("Exiting device", fg::red);
            break; // Exit the loop
        }
        raw_data[bytesRead] = '\0'; // Null-terminate the string
        std::string raw_input(raw_data, bytesRead);
        std::stringstream command_ss(raw_input);
        std::string command;
        // Loop through each command separated by line breaks
        // In case more commands were received at once; that usually doesn't happen in real operation
        while (std::getline(command_ss, command)) {
            if (!command.empty() && command.back() == '\r') {
                command.pop_back();
            }
            // Skip empty strings resulting from multiple line endings
            if (command.empty()) continue;
            cli::show_str("Processing command", command);
            // Get a response to the request
            std::string response = get_response(port, command, time_const, measurement_counter, data_frequency);
            // Send response if not empty
            if (response.empty()) continue;
            cli::show_str("Writing response", response);
            // Ensure the response ends with \r\n as expected
            std::string full_response = response;
            if (!full_response.ends_with("\r\n")) full_response += "\r\n";
            write(port, buffer(full_response));
        }
    } // end of while loop
} // end of mock_device function


///
/// Main function for the mock NPET device virtual machine.
/// Launches a mock NPET device that communicates over a specified COM port.
/// Requires virtual com ports to work!!!
int launch_vm() {
    try {
        constexpr int DEFAULT_FREQ = 100;
        int frequency{};
        io_context io;

        cli::echo("Mock NPET device virtual machine starting...", fg::cyan);
        // Use CLI prompt to get the COM port number as a string
        const std::string com_input = cli::prompt("Select COM port number [n]");
        cli::show_str("Selected COM port", com_input);
        // Construct the Windows-specific device path
        const std::string port_name = R"(\\.\COM)" + com_input;
        // Open a COM port
        serial_port port(io, port_name);
        // Set the frequency for data measurement
        try {
            const std::string frequency_input = cli::prompt("Data measurement frequency", std::to_string(DEFAULT_FREQ));
            if (frequency = std::stoi(frequency_input); frequency <= 0) {
                cli::err("Invalid frequency value. Using default: " + std::to_string(DEFAULT_FREQ));
                frequency = DEFAULT_FREQ;
            }
        } catch (const std::exception &e) {
            cli::err(
                "Invalid input for frequency. Using default: " + std::to_string(DEFAULT_FREQ) + ". Error: " +
                std::string(e.what()));
            frequency = DEFAULT_FREQ;
        }
        // Start the mock device function
        cli::echo("Mock NPET device virtual machine started", fg::green);
        device_loop(port, frequency);
        return 0;
    } catch (const std::exception &e) {
        cli::err("General error: " + std::string(e.what()));
        return 1;
    } // end of try-catch block
} // end of launch_vm function
