#ifndef MEASUREMENT_READER_DUAL_H
#define MEASUREMENT_READER_DUAL_H
#include <atomic>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>

#include "meas_reader.h"

class DualMeasReader;

struct DualMeasContext {
    int num_of_meas{5};
    // Run on its own thread while both legs are read (see NPETDual::readBatchMeasurements()).
    // Unlike MeasContext::monitor_fn (which watches a single leg's MeasReader), this one watches
    // the DualMeasReader that combines both legs, draining matched [meas_start, meas_stop] pairs
    // via DualMeasReader::grabMeasurement(). The two time constants are each leg's own, imported
    // up front so both are already known by the time this function starts running.
    std::function<void(DualMeasReader &, const DualMeasContext &, const Measurement &start_time_const,
                       const Measurement &stop_time_const)> monitor_fn = nullptr;
    // Directory saved measurements are written into; nullopt means don't save.
    std::optional<std::filesystem::path> save_dir{};
    Channel start_channel{Channel::CH1};
    Channel stop_channel{Channel::CH1};

    [[nodiscard]] std::string toString() const {
        return "dual_meas_context{num_of_meas: " + std::to_string(num_of_meas) +
               ", monitor_fn: " + (monitor_fn ? "set" : "null") +
               ", save_dir: " + (save_dir ? save_dir->string() : "null") +
               ", start_channel: " + std::to_string(static_cast<int>(start_channel)) +
               ", stop_channel: " + std::to_string(static_cast<int>(stop_channel)) + "}";
    }
};

// A matched pair of measurements, one from the START leg and one from the STOP leg,
// identified as belonging together by sharing the same meas_num.
struct DualMeasurement {
    Measurement meas_start;
    Measurement meas_stop;

    [[nodiscard]] std::string toString() const {
        return "dual_measurement{meas_start: " + meas_start.toString() +
               ", meas_stop: " + meas_stop.toString() + "}";
    }
};


///
/// Combines two independent MeasReader measurement streams (a START leg and a STOP leg) into a
/// single stream of matched [meas_start, meas_stop] pairs, keyed on Measurement::meas_num.
class DualMeasReader {
protected:
    // Guards pending_start_, pending_stop_, and for_monitor_q
    std::mutex combine_mtx_;
    // Measurements from one leg still waiting for their matching meas_num on the other leg
    std::unordered_map<int, Measurement> pending_start_;
    std::unordered_map<int, Measurement> pending_stop_;
    // Counts how many of the two legs (combine() invocations) are still running
    std::atomic<int> active_legs_{2};
    // Set once both legs have finished, so grabMeasurement() knows no further pairs are coming
    std::atomic<bool> stop_sign_{false};
    // Each leg's meas_num just before matching begins, used in matchMeasurement() to normalize
    // that leg's raw meas_num into a shared index space. The two physical NPETs run independent
    // free-running counters, so they are not guaranteed to be on the same meas_num on start.
    int start_baseline_{0};
    int stop_baseline_{0};

    ///
    /// Match a single measurement against the other leg's pending measurements, keyed on meas_num
    /// normalized against that leg's baseline (see start_baseline_/stop_baseline_).
    /// @param IS_START Whether MEAS came from the START leg (true) or the STOP leg (false)
    /// @param MEAS The measurement just read from the IS_START leg
    void matchMeasurement(bool IS_START, const Measurement &MEAS);

    ///
    /// Mark one leg as finished; once both legs have called this, signal grabMeasurement() to stop
    /// waiting and warn about any measurements that were left without a match.
    void finishLeg();

public:
    // Combined START/STOP measurement pairs
    std::queue<DualMeasurement> for_monitor_q{};

    DualMeasReader() = default;

    ///
    /// @param START_BASELINE meas_num of a measurement pulled from the START leg immediately
    /// before the batch starts, used to align the two legs' independent meas_num counters.
    /// @param STOP_BASELINE Same as START_BASELINE, for the STOP leg.
    DualMeasReader(const int START_BASELINE, const int STOP_BASELINE)
        : start_baseline_(START_BASELINE), stop_baseline_(STOP_BASELINE) {}

    ///
    /// Monitoring function for a Single NPET Measurement Reader,
    /// which passes the measurements into matchmaking with another MeasurementReader.
    /// @param IS_START Measurement Reader designation (START/STOP)
    /// @param reader Measurement Reader reference
    /// @param meas_set Measurement context struct
    /// @param time_const Time correction constant imported from NPET
    void combine(bool IS_START, MeasReader &reader, const MeasContext &meas_set, const Measurement &time_const);

    ///
    /// Method to grab measurement value for the monitoring function.
    /// @return A dual measurement from the Dual Measurement Reader.
    /// Is nullopt if the Dual Measurement has stopped.
    std::optional<DualMeasurement> grabMeasurement();
};


#endif //MEASUREMENT_READER_DUAL_H
