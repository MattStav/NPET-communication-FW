#include <cmath>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "NPET_comm.h"
#include <conio.h>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <queue>
#include <thread>

#include "meas_reader.h"

#include <spdlog/spdlog.h>


///
/// Grab data from NPET and put them in a queue for further processing.
/// Asynchronously reads data from the serial port and adds them to the queue.
/// Handles cancellation signal from keyboard input as well as cancellation from other threads.
/// Inserting a sentinel value (256) into the queue to signal the processor thread to stop.
void meas_reader::data_receiver() {
    std::array<unsigned char, NPET_comm::PACKET_SIZE> buf{};
    SPDLOG_DEBUG("Data receiver thread started");

    // Read exactly 13-byte packets from the serial port
    while (!stop_sign.load(std::memory_order_relaxed)) {
        boost::system::error_code ec;
        bool completed = false;

        // Read exactly 13 bytes - guarantees a complete packet
        boost::asio::async_read(npet.port, boost::asio::buffer(buf),
                                [&](const boost::system::error_code &error, const size_t _) {
                                    ec = error;
                                    completed = true;
                                });
        // Run until the async read completes
        npet.io.restart();
        while (!completed && !stop_sign.load(std::memory_order_relaxed)) {
            npet.io.poll_one();
        }
        // If the operation was aborted by error or another thread, exit the loop
        if (ec == boost::asio::error::operation_aborted || stop_sign.load(std::memory_order_relaxed)) {
            SPDLOG_DEBUG("Data receiver thread stopping ...");
            // Cancel pending operation BEFORE exiting
            npet.port.cancel();
            // Wait for the cancellation to complete
            npet.io.run();
            break;
        }
        if (!ec) {
            std::lock_guard lock(mtx_data);
            // Push each byte of the packet into the queue;
            // it's not possible to push all at once
            for (const unsigned char byte: buf) {
                received_data_q.push_back(byte);
            } // end of for loop
        }
    } // end of while loop
    stop_sign.store(true, std::memory_order_relaxed);
    SPDLOG_DEBUG("Data receiver thread stopped");
} // end of data_receiver_func function


///
/// Grab measurement bytes from the receiver queue for processing.
/// Grab the first two bytes and check if they match the measurement start sequence (1 followed by 11).
/// If they do, grab the next 13 bytes for processing. If not, discard the first byte and check again until the correct sequence is found.
/// If there is no data in the queue, wait until there is some data or a stop signal is received. If a stop signal is received
/// while waiting, return an empty array to signal the processor thread to stop immediately.
/// This ensures that the processor thread does not get stuck waiting for data when the program is trying to exit.
/// @return Array of 13 bytes containing the measurement data, or an empty array if a stop signal was received while waiting for data.
std::optional<std::array<uint8_t, 13> > meas_reader::grab_meas_from_receiver() {
    bool has_data = false;
    // Wait until there is some data in the queue or a stop signal is received

    while (true) {
        {
            // Code block to limit the scope of the lock
            std::lock_guard lock(mtx_data);
            has_data = !received_data_q.empty();
        }
        // If there is no data, then continue back
        if (!has_data) {
            // If there is no data, check for stop signal adn throw it to exit the processor thread immediately
            if (stop_sign.load(std::memory_order_relaxed)) {
                SPDLOG_DEBUG("Stop signal received while waiting for data, exiting ...");
                return std::nullopt;
            }
            // Otherwise, sleep briefly to avoid busy waiting and check again
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        // Check if the queue contains the measurement start sequence (1 followed by 11)
        // if so, grab the next 13 bytes for processing
        {
            // Code block to limit the scope of the lock
            std::lock_guard lock(mtx_data);
            const uint8_t first = received_data_q.front();
            // Peek at the second byte without removing
            auto it = received_data_q.begin();
            std::advance(it, 1);
            // If there is the correct sequence
            if (const uint8_t second = *it; first == 1 && second == 11) {
                std::array<uint8_t, 13> measurement_set{};
                // Found the sequence! Grab all 13 bytes
                for (int i = 0; i < 13; i++) {
                    measurement_set[i] = received_data_q.front();
                    received_data_q.pop_front();
                }
                return measurement_set;
            }
            // Not the right sequence, discard the first byte and try again
            received_data_q.pop_front();
        } // end of block with lock
    } // end of while loop waiting for data
} // end of grab_data_from_queue function


///
/// Process the data received from NPET.
/// @param meas_set Measurement context struct
/// /// Contains the number of measurements, display and save flags, and channel number
/// @param time_const Time correction constant imported from NPET
void meas_reader::data_processor(const meas_context &meas_set, const measurement &time_const) {
    const __float128 multiplier = get_measurement_multiplier(npet.fw_version);
    int meas_counter = 0; // Track total including overflows
    SPDLOG_DEBUG("Data processor thread started");
    SPDLOG_DEBUG("Data processor time const: {}", time_const.to_string());
    SPDLOG_DEBUG("Data processor measurement context : {} measurements, channel {} , monitoring {}, save {}",
                 meas_set.num_of_meas, meas_set.channel, meas_set.monitor_fn ? "true" : "false", meas_set.save);
    while (!aborted.load(std::memory_order_relaxed)) {
        // Grab the next measurement set from the receiver queue
        auto measurement_res_raw = grab_meas_from_receiver();
        // If the returned array is empty,
        // it means a stop signal was received while waiting for data, so we exit the loop immediately
        if (!measurement_res_raw) break;
        measurement measurement_res;
        meas_counter++;
        try {
            measurement_res = process_measurement(*measurement_res_raw, multiplier, time_const);
        } catch (const std::exception &) {
            corrupted.fetch_add(1, std::memory_order_relaxed);
            SPDLOG_WARN("Corrupted measurement received, discarding. Corrupted measurements: {}",
                         corrupted.load(std::memory_order_relaxed));
            continue;
        } {
            // Code block to limit the scope of the lock
            std::lock_guard lock(mtx_data);
            for_saver_q.push(measurement_res);
            for_monitor_q.push(measurement_res);
        }
        // If target meas number is reached, end the measurement sequence. Except in infinite operation.
        if (meas_counter == meas_set.num_of_meas && meas_counter != INFINITE_OP) {
            SPDLOG_DEBUG("Target number of measurements reached, ending measurement sequence");
            break;
        }
    } // end of while loop
    stop_sign.store(true, std::memory_order_relaxed);
    SPDLOG_DEBUG("Data processor thread stopped");
} // end of data_processor_func function


std::optional<measurement> meas_reader::grab_meas_from_processor(std::queue<measurement> &q) {
    while (true) {
        {
            std::lock_guard lock(mtx_data);
            // Check if there's data available
            if (!q.empty()) {
                measurement ret = q.front();
                q.pop();
                return ret; // Return happens inside the lock scope
            }
        }
        // No data available
        if (stop_sign.load(std::memory_order_relaxed)) {
            SPDLOG_DEBUG("Stop signal received while waiting for data, exiting ...");
            return std::nullopt;
        }
        // Sleep briefly to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } // end of while loop waiting for data
} // end of grab_measurement_from_processor function


void meas_reader::data_saver(const int channel_num) {
    std::ofstream output_file;
    SPDLOG_DEBUG("Data saver thread started");

    // Open the correct file depending on the channel
    output_file.open(output_file_path(channel_num));
    if (!output_file.is_open()) {
        // If the file cannot be opened, stop the program and show an error message
        stop_sign.store(true, std::memory_order_relaxed);
        SPDLOG_ERROR("failed to open output file: {}", output_file.getloc().name());
        throw std::runtime_error("failed to generate output file");
    }
    while (true) { // Data saver never exits early, so no data is ever lost
        std::optional<measurement> meas = grab_meas_from_processor(for_saver_q);
        if (!meas) break;
        output_file << meas.value().intp << " " << std::fixed << float128_to_string(meas.value().fracp) << std::endl;
    } // end of while loop
    if (output_file.is_open()) output_file.close(); // redundant, but good practice
    SPDLOG_DEBUG("Data saver thread stopped");
} // end of data_saver function


///
/// Read measurements from NPET.
/// Sets the NPEt to start streaming a number of measurements from a specified channel.
/// Starts the threads to process the measurements.
/// @param meas_set Measurement context struct
/// /// Contains the number of measurements, monitor function, a save flag, and channel number
void meas_reader::main(const meas_context &meas_set) {
    SPDLOG_INFO("Initiating Measurement Reader ...");
    assert(npet.port.is_open());
    // Keyboard watcher: ESC => request stop + cancel any blocking read.
    std::jthread key_watcher([this] {
        SPDLOG_DEBUG("Keyboard watcher (Esc) thread started");
        while (!stop_sign.load(std::memory_order_relaxed)) {
            if (_kbhit()) {
                if (const int ch = _getch(); ch == 27) {
                    // ESC
                    SPDLOG_DEBUG("Keyboard watcher triggered, Measurement Reader stopping ...");
                    aborted.store(true, std::memory_order_relaxed);
                    stop_sign.store(true, std::memory_order_relaxed);
                    break;
                }
            }
            // Sleep inbetween checks, this doesnt need to respond very fast
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        } // end of while loop
    }); // end of key_watcher thread
    // Import the time constant from NPET
    const measurement time_const = npet.import_time_constant();
    assert(time_const.meas_num == -1);
    SPDLOG_DEBUG("Time constant imported from NPET: {}", time_const.to_string());
    auto receiver = std::jthread(&meas_reader::data_receiver, this);
    auto processor = std::jthread(&meas_reader::data_processor, this, meas_set, time_const);
    std::thread saver;
    if (meas_set.save) saver = std::thread(&meas_reader::data_saver, this, meas_set.channel);
    std::thread monitor;
    if (meas_set.monitor_fn) {
        monitor = std::thread(
            meas_set.monitor_fn,
            std::ref(*this),
            std::cref(meas_set),
            std::cref(time_const)
        );
    }
    SPDLOG_DEBUG("All threads started");
    // Start the NPET measurements
    npet.port.write_some(boost::asio::buffer(get_measurement_cmd(meas_set.channel, meas_set.num_of_meas)));
    SPDLOG_DEBUG("Measurement command sent to NPET, waiting for threads to finish ...");
    if (saver.joinable()) saver.join();
    if (monitor.joinable()) monitor.join();
} // end of read_measurements function


///
/// Clean up after the measurement stream.
void meas_reader::end_sequence() const {
    SPDLOG_INFO("Cleaning up after Measurement Reader ...");
    if (!npet.is_responsive(true)) SPDLOG_WARN("Failed to immediately terminate measurement stream");
}
