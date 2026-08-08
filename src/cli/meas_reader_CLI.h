#ifndef MEASUREMENT_READER_CLI_H
#define MEASUREMENT_READER_CLI_H

#include "meas_reader.h"
#include "meas_reader_dual.h"

struct MeasExtended : Measurement { // Inherits from measurement
    std::string processed_str; // Formatted string of the measurement for display

    [[nodiscard]] bool isProcessed() const { return !processed_str.empty(); }

    [[nodiscard]] float approxValue() const {
        return static_cast<float>(intp) + static_cast<float>(fracp);
    }

    // Default constructor
    MeasExtended() = default;

    // Constructor to initialize the meas_extended struct from measurement struct
    explicit MeasExtended(const Measurement &m) : Measurement(m) {
    }
};

void readerCliSync(
    MeasReader &reader,
    const MeasContext &meas_set,
    const Measurement &time_const);

void readerCliAdvanced(
    MeasReader &reader,
    const MeasContext &meas_set,
    const Measurement &time_const);

void readerCliBasic(
    MeasReader &reader,
    const MeasContext &meas_set,
    const Measurement &time_const);

void dualReaderCliBasic(
    DualMeasReader &dual_reader,
    const DualMeasContext &meas_set,
    const Measurement &start_time_const,
    const Measurement &stop_time_const);

#endif
