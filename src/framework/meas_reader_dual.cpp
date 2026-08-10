#include "meas_reader_dual.h"

#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>


void DualMeasReader::matchMeasurement(const bool IS_START, const Measurement &MEAS) {
    std::scoped_lock const LOCK(combine_mtx_);
    // meas_num is a free-running byte counter on the device, so normalize against this leg's own
    // baseline (wrapping via uint8_t) before using it as the shared matching key.
    const int BASELINE = IS_START ? start_baseline_ : stop_baseline_;
    const int KEY = static_cast<uint8_t>(MEAS.meas_num - BASELINE);
    auto &other_pending = IS_START ? pending_stop_ : pending_start_;
    if (const auto IT = other_pending.find(KEY); IT != other_pending.end()) {
        for_monitor_q.push(IS_START
                               ? DualMeasurement{.meas_start = MEAS, .meas_stop = IT->second}
                               : DualMeasurement{.meas_start = IT->second, .meas_stop = MEAS});
        other_pending.erase(IT);
    } else {
        auto &own_pending = IS_START ? pending_start_ : pending_stop_;
        own_pending.emplace(KEY, MEAS);
    }
} // end of matchMeasurement function


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


void DualMeasReader::combine(const bool IS_START, MeasReader &reader, const MeasContext & /*meas_set*/,
                             const Measurement & /*time_const*/) {
    (IS_START ? start_meas_reader_ : stop_meas_reader_).store(&reader, std::memory_order_relaxed);
    while (!reader.aborted.load(std::memory_order_relaxed)) {
        const std::optional<Measurement> MEAS = reader.grabMeasFromProcessor(reader.for_monitor_q);
        if (!MEAS) {
            break;
        }
        matchMeasurement(IS_START, *MEAS);
    } // end of while loop
    if (reader.aborted.load(std::memory_order_relaxed)) {
        // This leg's own key_watcher caught the Esc press; the other leg's key_watcher is racing
        // against it on the same keyboard input and may never see it, so stop that leg directly
        // rather than requiring a second Esc press.
        if (MeasReader *other = (IS_START ? stop_meas_reader_ : start_meas_reader_).load(std::memory_order_relaxed)) {
            other->aborted.store(true, std::memory_order_relaxed);
            other->stop_sign.store(true, std::memory_order_relaxed);
        }
    }
    finishLeg();
} // end of combine function


std::optional<DualMeasurement> DualMeasReader::grabMeasurement() {
    while (true) {
        if (stop_sign_.load(std::memory_order_relaxed)) {
            return std::nullopt;
        }
        {
            std::scoped_lock const LOCK(combine_mtx_);
            if (!for_monitor_q.empty()) {
                DualMeasurement ret = for_monitor_q.front();
                for_monitor_q.pop();
                return ret;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } // end of while loop waiting for data
} // end of grabMeasurement function
