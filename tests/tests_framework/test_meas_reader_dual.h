#ifndef TEST_MEASUREMENT_READER_DUAL_H
#define TEST_MEASUREMENT_READER_DUAL_H

#include <gtest/gtest.h>
#include "meas_reader_dual.h"

// combine_mtx_/pending_start_/pending_stop_/active_legs_/stop_sign_/start_baseline_/stop_baseline_/
// matchMeasurement()/finishLeg() are protected on DualMeasReader; expose them for direct
// inspection/manipulation in tests. This lets the matching/finishing logic be exercised without a
// live MeasReader, which combine() itself needs (and which requires real hardware or a
// virtual-machine connection to construct) - so combine()'s Esc-propagation between legs is not
// covered here; see the workflow-level dual tests for that.
class DualMeasReaderFixture : public ::testing::Test, public DualMeasReader {
protected:
    using DualMeasReader::pending_start_;
    using DualMeasReader::pending_stop_;
    using DualMeasReader::active_legs_;
    using DualMeasReader::stop_sign_;
    using DualMeasReader::start_baseline_;
    using DualMeasReader::stop_baseline_;
    using DualMeasReader::matchMeasurement;
    using DualMeasReader::finishLeg;
};

#endif //TEST_MEASUREMENT_READER_DUAL_H
