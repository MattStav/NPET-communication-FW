#include "meas_reader_dual.h"

#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>


///
/// Clear all combining state left over from a previous run.
void DualMeasReader::reset() {
    std::scoped_lock const LOCK(combine_mtx_);
    pending_start_.clear();
    pending_stop_.clear();
    for_monitor_q = {};
    active_legs_.store(2, std::memory_order_relaxed);
    stop_sign_.store(false, std::memory_order_relaxed);
} // end of reset function


///
/// Match a single measurement against the other leg's pending measurements by meas_num.
/// @param IS_START Whether MEAS came from the START leg (true) or the STOP leg (false)
/// @param MEAS The measurement just read from the IS_START leg
void DualMeasReader::matchMeasurement(const bool IS_START, const Measurement &MEAS) {
    std::scoped_lock const LOCK(combine_mtx_);
    auto &other_pending = IS_START ? pending_stop_ : pending_start_;
    if (const auto IT = other_pending.find(MEAS.meas_num); IT != other_pending.end()) {
        for_monitor_q.push(IS_START
            ? DualMeasurement{.meas_start = MEAS, .meas_stop = IT->second}
            : DualMeasurement{.meas_start = IT->second, .meas_stop = MEAS});
        other_pending.erase(IT);
    } else {
        auto &own_pending = IS_START ? pending_start_ : pending_stop_;
        own_pending.emplace(MEAS.meas_num, MEAS);
    }
} // end of matchMeasurement function


///
/// Mark one leg as finished; once both legs have called this, signal grabMeasurement() to stop
/// waiting and warn about any measurements that were left without a match.
void DualMeasReader::finishLeg() {
    if (active_legs_.fetch_sub(1, std::memory_order_relaxed) == 1) {
        std::scoped_lock const LOCK(combine_mtx_);
        if (!pending_start_.empty() || !pending_stop_.empty()) {
            SPDLOG_WARN(
                "Dual measurement combine finished with {} unmatched start and {} unmatched stop measurement(s)",
                pending_start_.size(), pending_stop_.size());
        }
        stop_sign_.store(true, std::memory_order_relaxed);
    }
} // end of finishLeg function


///
/// Monitoring function for a Single NPET MMeasurement Reader,
/// which passes the measurements into matchmaking with another MeasurementReader.
/// @param IS_START Measurement Reader designation (START/STOP)
/// @param reader Measurement Reader reference
void DualMeasReader::combine(const bool IS_START, MeasReader &reader, const MeasContext & /*meas_set*/,
                             const Measurement & /*time_const*/) {
    while (true) {
        const std::optional<Measurement> MEAS = reader.grabMeasFromProcessor(reader.for_monitor_q);
        if (!MEAS) {
            break;
        }
        matchMeasurement(IS_START, *MEAS);
    } // end of while loop
    finishLeg();
} // end of combine function


///
/// @return A dual measurement from the Dual Measurement Reader.
/// Is nullopt if the Dual Measurement has stopped.
std::optional<DualMeasurement> DualMeasReader::grabMeasurement() {
    while (true) {
        {
            std::scoped_lock const LOCK(combine_mtx_);
            if (!for_monitor_q.empty()) {
                DualMeasurement ret = for_monitor_q.front();
                for_monitor_q.pop();
                return ret;
            }
        }
        if (stop_sign_.load(std::memory_order_relaxed)) {
            return std::nullopt;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } // end of while loop waiting for data
} // end of grabMeasurement function
