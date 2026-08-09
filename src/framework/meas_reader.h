#ifndef MEASUREMENT_READER_H
#define MEASUREMENT_READER_H
#include <queue>
#include <mutex>
#include <atomic>
#include <filesystem>
#include <functional>
#include <optional>

#include "meas_func.h"

class MeasReader;

struct MeasContext {
    int num_of_meas{5};
    std::function<void(MeasReader &, const MeasContext &, const Measurement &)> monitor_fn = nullptr;
    // Full path of the file measurements are written into ; nullopt means don't save.
    std::optional<std::filesystem::path> save_path{};
    Channel channel{Channel::CH1};

    [[nodiscard]] std::string toString() const {
        return "meas_context{num_of_meas: " + std::to_string(num_of_meas) +
               ", monitor_fn: " + (monitor_fn ? "set" : "null") +
               ", save_path: " + (save_path ? save_path->string() : "null") +
               ", channel: " + std::to_string(static_cast<int>(channel)) + "}";
    }
};

// Forward declaration instead of #include "NPET_comm.h"
class NPETComm;

class MeasReader {
    NPETComm &npet_; // a reference to the NPET communicator instance
    std::mutex mtx_data_; // a mutex to handle queue accesses
    // Queue to store the data received from NPET
    std::deque<std::uint8_t> received_data_q_{};
    // Queue to store processed measurements
    std::queue<Measurement> for_saver_q_{};

    ///
    /// Grab data from NPET and put them in a queue for further processing.
    /// Asynchronously reads data from the serial port and adds them to the queue.
    /// Handles cancellation signal from keyboard input as well as cancellation from other threads.
    /// Inserting a sentinel value (256) into the queue to signal the processor thread to stop.
    void dataReceiver();

    ///
    /// Grab measurement bytes from the receiver queue for processing.
    /// Grab the first two bytes and check if they match the measurement start sequence (1 followed by 11).
    /// If they do, grab the next 13 bytes for processing. If not, discard the first byte and check again until the correct sequence is found.
    /// If there is no data in the queue, wait until there is some data or a stop signal is received. If a stop signal is received
    /// while waiting, return an empty array to signal the processor thread to stop immediately.
    /// This ensures that the processor thread does not get stuck waiting for data when the program is trying to exit.
    /// @return Array of 13 bytes containing the measurement data, or an empty array if a stop signal was received while waiting for data.
    std::optional<std::array<uint8_t, 13> > grabMeasFromReceiver();

    ///
    /// Process the data received from NPET.
    /// @param meas_set Measurement context struct
    /// /// Contains the number of measurements, display and save flags, and channel number
    /// @param time_const Time correction constant imported from NPET
    void dataProcessor(const MeasContext &meas_set, const Measurement &time_const);

    void dataSaver(const std::filesystem::path &save_path);

    ///
    /// Read measurements from NPET.
    /// Sets the NPEt to start streaming a number of measurements from a specified channel.
    /// Starts the threads to process the measurements.
    /// @param meas_set Measurement context struct
    /// /// Contains the number of measurements, monitor function, a save flag, and channel number
    void main(const MeasContext &meas_set);

    ///
    /// Clean up after the measurement stream.
    void endSequence() const;

public:
    // Queue to store processed measurements
    std::queue<Measurement> for_monitor_q{};
    // Signal to stop the measurement
    std::atomic<bool> stop_sign{false};
    // Signal that user aborted the measurement
    std::atomic<bool> aborted{false};
    // Number of corrupted measurements
    std::atomic<int> corrupted;

    std::optional<Measurement> grabMeasFromProcessor(std::queue<Measurement> &q);

    size_t receiverQSize() {
        std::scoped_lock const LOCK(mtx_data_);
        return received_data_q_.size();
    }

    size_t saverQSize() {
        std::scoped_lock const LOCK(mtx_data_);
        return for_saver_q_.size();
    }

    size_t monitorQSize() {
        std::scoped_lock const LOCK(mtx_data_);
        return for_monitor_q.size();
    }

    // Constructor to begin reading measurements, this class does nothing else
    explicit MeasReader(NPETComm &npet, const MeasContext &meas_set) : npet_(npet) {
        main(meas_set);
    }

    MeasReader(const MeasReader &) = delete;

    MeasReader &operator=(const MeasReader &) = delete;

    MeasReader(MeasReader &&) = delete;

    MeasReader &operator=(MeasReader &&) = delete;

    // Destructor to end the measurement sequence
    ~MeasReader() { endSequence(); }
}; // end of measurement_reader class


///
/// Start a measurement on NPET using the supplied measurement context.
/// @param npet NPETComm instance to start the measurement on
/// @param meas_set The measurement context containing the settings for the measurement
inline void startMeasurement(NPETComm &npet, MeasContext const &meas_set) {
    [[maybe_unused]] MeasReader const SESSION(npet, meas_set);
}


#endif //MEASUREMENT_READER_H
