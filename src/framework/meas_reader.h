#ifndef MEASUREMENT_READER_H
#define MEASUREMENT_READER_H
#include <queue>
#include <mutex>
#include <atomic>
#include <functional>

#include "meas_func.h"

class MeasReader;

struct MeasContext {
    int num_of_meas{5};
    std::function<void(MeasReader &, const MeasContext &, const Measurement &)> monitor_fn = nullptr;
    bool save{false};
    int channel{1};

    [[nodiscard]] std::string toString() const {
        return "meas_context{num_of_meas: " + std::to_string(num_of_meas) +
               ", monitor_fn: " + (monitor_fn ? "set" : "null") +
               ", save: " + (save ? "true" : "false") +
               ", channel: " + std::to_string(channel) + "}";
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

    void dataReceiver();

    std::optional<std::array<uint8_t, 13> > grabMeasFromReceiver();

    void dataProcessor(const MeasContext &meas_set, const Measurement &time_const);

    void dataSaver(int CHANNEL_NUM);

    // Main function to read measurements from NPET, this is called in the constructor and starts the measurement sequence
    void main(const MeasContext &meas_set);

    // End the measurement sequence, this is necessary, otherwise the program will crash
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


#endif //MEASUREMENT_READER_H
