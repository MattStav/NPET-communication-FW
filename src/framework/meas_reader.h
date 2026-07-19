#ifndef MEASUREMENT_READER_H
#define MEASUREMENT_READER_H
#include <queue>
#include <mutex>
#include <atomic>
#include <functional>

#include "meas_func.h"

class meas_reader;

struct meas_context {
    int num_of_meas{5};
    std::function<void(meas_reader &, const meas_context &, const measurement &)> monitor_fn = nullptr;
    bool save{false};
    int channel{1};

    [[nodiscard]] inline std::string to_string() const {
        return "meas_context{num_of_meas: " + std::to_string(num_of_meas) +
               ", monitor_fn: " + (monitor_fn ? "set" : "null") +
               ", save: " + (save ? "true" : "false") +
               ", channel: " + std::to_string(channel) + "}";
    }
};

// Forward declaration instead of #include "NPET_comm.h"
class NPET_comm;

class meas_reader {
    NPET_comm &npet; // a reference to the NPET communicator instance
    std::mutex mtx_data; // a mutex to handle queue accesses
    // Queue to store the data received from NPET
    std::deque<std::uint8_t> received_data_q;
    // Queue to store processed measurements
    std::queue<measurement> for_saver_q;

    void data_receiver();

    std::optional<std::array<uint8_t, 13> > grab_meas_from_receiver();

    void data_processor(const meas_context &meas_set, const measurement &time_const);

    void data_saver(int channel_num);

    // Main function to read measurements from NPET, this is called in the constructor and starts the measurement sequence
    void main(const meas_context &meas_set);

    // End the measurement sequence, this is necessary, otherwise the program will crash
    void end_sequence() const;

public:
    // Queue to store processed measurements
    std::queue<measurement> for_monitor_q;
    // Signal to stop the measurement
    std::atomic<bool> stop_sign{false};
    // Signal that user aborted the measurement
    std::atomic<bool> aborted{false};
    // Number of corrupted measurements
    std::atomic<int> corrupted;

    std::optional<measurement> grab_meas_from_processor(std::queue<measurement> &q);

    size_t receiver_q_size() {
        std::lock_guard lock(mtx_data);
        return received_data_q.size();
    }

    size_t saver_q_size() {
        std::lock_guard lock(mtx_data);
        return for_saver_q.size();
    }

    size_t monitor_q_size() {
        std::lock_guard lock(mtx_data);
        return for_monitor_q.size();
    }

    // Constructor to begin reading measurements, this class does nothing else
    explicit meas_reader(NPET_comm &npet, const meas_context &meas_set) : npet(npet) {
        main(meas_set);
    }

    // Destructor to end the measurement sequence
    ~meas_reader() { end_sequence(); }
}; // end of measurement_reader class


#endif //MEASUREMENT_READER_H
