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
void MeasReader::dataReceiver() {
    std::array<unsigned char, MEASUREMENT_PACKET_SIZE> buf{};
    SPDLOG_DEBUG("Data receiver thread started");

    // Read exactly 13-byte packets from the serial port
    while (!stop_sign.load(std::memory_order_relaxed)) {
        boost::system::error_code ec;
        bool completed = false;

        // Read exactly 13 bytes - guarantees a complete packet
        boost::asio::async_read(npet_.getPort(), boost::asio::buffer(buf),
                                [&](const boost::system::error_code &error, const size_t /*_*/) {
                                    ec = error;
                                    completed = true;
                                });
        // Run until the async read completes
        npet_.pollUntil([&] { return completed || stop_sign.load(std::memory_order_relaxed); });
        // If the operation was aborted by error or another thread, exit the loop
        if (ec == boost::asio::error::operation_aborted || stop_sign.load(std::memory_order_relaxed)) {
            SPDLOG_DEBUG("Data receiver thread stopping ...");
            // Cancel pending operation BEFORE exiting and wait for the cancellation to complete
            npet_.cancelPendingOperation();
            break;
        }
        if (!ec) {
            std::scoped_lock const LOCK(mtx_data_);
            // Push each byte of the packet into the queue;
            // it's not possible to push all at once
            for (const unsigned char BYTE: buf) {
                received_data_q_.push_back(BYTE);
            } // end of for loop
        }
    } // end of while loop
    stop_sign.store(true, std::memory_order_relaxed);
    SPDLOG_DEBUG("Data receiver thread stopping, ending measurement stream ...");
    (void) npet_.isResponsive(true);
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
std::optional<std::array<uint8_t, 13> > MeasReader::grabMeasFromReceiver() {
    bool has_data = false;
    // Wait until there is some data in the queue or a stop signal is received

    while (true) {
        {
            // Code block to limit the scope of the lock
            std::scoped_lock const LOCK(mtx_data_);
            has_data = !received_data_q_.empty();
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
            std::scoped_lock const LOCK(mtx_data_);
            const uint8_t FIRST = received_data_q_.front();
            // Peek at the second byte without removing
            auto it = received_data_q_.begin();
            std::advance(it, 1);
            // If there is the correct sequence
            if (const uint8_t SECOND = *it; FIRST == 1 && SECOND == 11) {
                std::array<uint8_t, 13> measurement_set{};
                // Found the sequence! Grab all 13 bytes
                for (int i = 0; i < 13; i++) {
                    measurement_set.at(i) = received_data_q_.front();
                    received_data_q_.pop_front();
                }
                return measurement_set;
            }
            // Not the right sequence, discard the first byte and try again
            received_data_q_.pop_front();
        } // end of block with lock
    } // end of while loop waiting for data
} // end of grab_data_from_queue function


///
/// Process the data received from NPET.
/// @param meas_set Measurement context struct
/// /// Contains the number of measurements, display and save flags, and channel number
/// @param time_const Time correction constant imported from NPET
void MeasReader::dataProcessor(const MeasContext &meas_set, const Measurement &time_const) {
    const __float128 MULTIPLIER = npet_.fw_version.getMultiplier();
    int meas_counter = 0; // Track total including overflows
    SPDLOG_DEBUG("Data processor thread started");
    SPDLOG_DEBUG("Data processor time const: {}", time_const.toString());
    SPDLOG_DEBUG("Data processor measurement context : {} measurements, channel {} , monitoring {}, save path {}",
                 meas_set.num_of_meas, static_cast<int>(meas_set.channel), meas_set.monitor_fn ? "true" : "false",
                 meas_set.save_path ? meas_set.save_path->string() : "none");
    while (!aborted.load(std::memory_order_relaxed)) {
        // Grab the next measurement set from the receiver queue
        auto measurement_res_raw = grabMeasFromReceiver();
        // If the returned array is empty,
        // it means a stop signal was received while waiting for data, so we exit the loop immediately
        if (!measurement_res_raw) {
            break;
        }
        Measurement measurement_res;
        meas_counter++;
        try {
            measurement_res = decodeMeasurementSet(*measurement_res_raw, MULTIPLIER, time_const);
        } catch (const std::exception &) {
            corrupted.fetch_add(1, std::memory_order_relaxed);
            SPDLOG_WARN("Corrupted measurement received, discarding. Corrupted measurements: {}",
                        corrupted.load(std::memory_order_relaxed));
            continue;
        } {
            // Code block to limit the scope of the lock
            std::scoped_lock const LOCK(mtx_data_);
            for_saver_q_.push(measurement_res);
            for_monitor_q.push(measurement_res);
        }
        // If target meas number is reached, end the measurement sequence. Except in infinite operation.
        if (meas_counter == meas_set.num_of_meas && meas_counter != INFINITE_OPERATION) {
            SPDLOG_DEBUG("Target number of measurements reached, ending measurement sequence");
            break;
        }
    } // end of while loop
    stop_sign.store(true, std::memory_order_relaxed);
    SPDLOG_DEBUG("Data processor thread stopped");
} // end of data_processor_func function


std::optional<Measurement> MeasReader::grabMeasFromProcessor(std::queue<Measurement> &q) {
    while (true) {
        {
            std::scoped_lock const LOCK(mtx_data_);
            // Check if there's data available
            if (!q.empty()) {
                Measurement ret = q.front();
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


void MeasReader::dataSaver(const std::filesystem::path &save_path) {
    std::ofstream output_file;
    SPDLOG_DEBUG("Data saver thread started");

    SPDLOG_DEBUG("Saving measurements to: {}", save_path.string());
    output_file.open(save_path);
    if (!output_file.is_open()) {
        // If the file cannot be opened, stop the program and show an error message
        stop_sign.store(true, std::memory_order_relaxed);
        SPDLOG_ERROR("failed to open output file: {}", save_path.string());
        throw std::runtime_error("failed to generate output file");
    }
    while (true) {
        // Data saver never exits early, so no data is ever lost
        std::optional<Measurement> meas = grabMeasFromProcessor(for_saver_q_);
        if (!meas) {
            break;
        }
        output_file << meas.value().intp << " " << std::fixed << float128ToString(meas.value().fracp) << '\n';
    } // end of while loop
    if (output_file.is_open()) {
        output_file.close(); // redundant, but good practice
    }
    SPDLOG_DEBUG("Data saver thread stopped");
} // end of data_saver function


///
/// Read measurements from NPET.
/// Sets the NPEt to start streaming a number of measurements from a specified channel.
/// Starts the threads to process the measurements.
/// @param meas_set Measurement context struct
/// /// Contains the number of measurements, monitor function, a save flag, and channel number
void MeasReader::main(const MeasContext &meas_set) {
    SPDLOG_INFO("Initiating Measurement Reader ...");
    assert(npet_.isOpen());
    // Import the time constant from NPET
    const Measurement TIME_CONST = npet_.importTimeConstant();
    assert(TIME_CONST.meas_num == -1);
    SPDLOG_DEBUG("Time constant imported from NPET: {}", TIME_CONST.toString());
    auto receiver = std::jthread(&MeasReader::dataReceiver, this);
    auto processor = std::jthread(&MeasReader::dataProcessor, this, meas_set, TIME_CONST);
    std::jthread saver;
    if (meas_set.save_path) {
        saver = std::jthread(&MeasReader::dataSaver, this, *meas_set.save_path);
    }
    std::jthread monitor;
    if (meas_set.monitor_fn) {
        monitor = std::jthread(
            meas_set.monitor_fn,
            std::ref(*this),
            std::cref(meas_set),
            std::cref(TIME_CONST)
        );
    }
    // Keyboard watcher: ESC => request stop + cancel any blocking read.
    std::jthread key_watcher([this, &receiver, &processor, &saver, &monitor] {
        SPDLOG_DEBUG("Keyboard watcher (Esc) thread started");
        auto any_alive = [&] {
            return receiver.joinable() || processor.joinable()
                   || saver.joinable() || monitor.joinable();
        };
        while (any_alive()) {
            if (_kbhit()) {
                constexpr int ESCAPE_KEY = 27;
                if (const int CH = _getch(); CH == ESCAPE_KEY) {
                    SPDLOG_DEBUG("Keyboard watcher triggered, Measurement Reader stopping ...");
                    aborted.store(true, std::memory_order_relaxed);
                    stop_sign.store(true, std::memory_order_relaxed);
                }
            }
            // Sleep inbetween checks, this doesnt need to respond very fast
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        } // end of while loop
        SPDLOG_DEBUG("Keyboard watcher (Esc) thread stopped");
    }); // end of key_watcher thread
    SPDLOG_DEBUG("All threads started");
    // Start the NPET measurements
    npet_.writeToSerial(getMeasurementCmd(meas_set.channel, meas_set.num_of_meas));
    SPDLOG_DEBUG("Measurement command sent to NPET, waiting for threads to finish ...");
    // Join the workers first, to allow key_watcher loop to finish
    if (receiver.joinable()) {
        receiver.join();
    }
    if (processor.joinable()) {
        processor.join();
    }
    if (saver.joinable()) {
        saver.join();
    }
    if (monitor.joinable()) {
        monitor.join();
    }
    key_watcher.join();
    SPDLOG_DEBUG("All threads finished");
} // end of read_measurements function


///
/// Clean up after the measurement stream.
void MeasReader::endSequence() const {
    SPDLOG_INFO("Cleaning up after Measurement Reader ...");
    if (!npet_.isResponsive(true)) {
        SPDLOG_WARN("Failed to immediately terminate measurement stream");
    }
}
