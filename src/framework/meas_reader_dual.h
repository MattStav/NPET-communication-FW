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
/// One instance is meant to be reused across repeated start/stop measurement runs; reset() clears
/// state left over from a previous run.
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

    // Matches MEAS (just read from the IS_START leg) against the other leg's pending
    // measurements by meas_num. Pushes the combined [meas_start, meas_stop] pair onto
    // for_monitor_q if the other half has already arrived, otherwise stashes MEAS to wait for it.
    void matchMeasurement(bool IS_START, const Measurement &MEAS);

    // Marks one leg as finished. Once both legs have called this, sets stop_sign_ so
    // grabMeasurement() stops waiting, and warns about any measurements that never found a match.
    void finishLeg();

public:
    // Combined START/STOP measurement pairs
    std::queue<DualMeasurement> for_monitor_q{};

    // Clear all state left over from a previous run. Call before starting a new pair of legs.
    void reset();

    // Monitor function meant to be installed on both legs' MeasContext::monitor_fn, once per leg
    // (distinguished by IS_START). Drains the calling leg's own reader.for_monitor_q via
    // matchMeasurement() until the leg is done, then calls finishLeg().
    void combine(bool IS_START, MeasReader &reader, const MeasContext &meas_set, const Measurement &time_const);

    // Pop the next combined measurement pair, blocking until one is available or both legs have
    // finished with nothing left to match.
    std::optional<DualMeasurement> grabMeasurement();
};


#endif //MEASUREMENT_READER_DUAL_H
