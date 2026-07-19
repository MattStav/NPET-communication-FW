#ifndef MEASUREMENT_READER_CLI_H
#define MEASUREMENT_READER_CLI_H

#include "meas_reader.h"

struct meas_extended : measurement { // Inherits from measurement
    std::string processed_str{}; // Formatted string of the measurement for display

    [[nodiscard]] bool is_processed() const { return !processed_str.empty(); }

    [[nodiscard]] float approx_value() const {
        return static_cast<float>(intp) + static_cast<float>(fracp);
    }

    // Default constructor
    meas_extended() = default;

    // Constructor to initialize the meas_extended struct from measurement struct
    explicit meas_extended(const measurement &m) : measurement(m) {
    }
};

void reader_cli_sync(
    meas_reader &reader,
    const meas_context &meas_set,
    const measurement &time_const);

void reader_cli_advanced(
    meas_reader &reader,
    const meas_context &meas_set,
    const measurement &time_const);

void reader_cli_basic(
    meas_reader &reader,
    const meas_context &meas_set,
    const measurement &time_const);

#endif
