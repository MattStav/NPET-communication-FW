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

///
/// Display the measurement read from NPET only as hh:mm:ss with the fractional part rounded.
/// @param reader Reference to the measurement_reader object that is reading measurements from the NPET device
/// @param meas_set Reference to the measurement context
/// @param time_const Reference to the time correction constant imported from the NPET device
void readerCliSync(
    MeasReader &reader,
    const MeasContext &meas_set,
    const Measurement &time_const);

///
/// Advanced CLI measurement monitor showing extended information about ongoing measurement.
/// @param reader Reference to the measurement_reader object that is reading measurements from the NPET device
/// @param meas_set Reference to the measurement context
/// @param time_const Reference to the time correction constant imported from the NPET device
void readerCliAdvanced(
    MeasReader &reader,
    const MeasContext &meas_set,
    const Measurement &time_const);

///
/// Display only a progress bar of the measurement process.
/// @param reader Reference to the measurement_reader object that is reading measurements from the NPET device
/// @param meas_set Reference to the measurement context
/// @param time_const Reference to the time correction constant imported from the NPET device
void readerCliBasic(
    MeasReader &reader,
    const MeasContext &meas_set,
    const Measurement &time_const);

///
/// Display only a progress bar of the combined dual measurement process.
/// @param dual_reader Reference to the dual measurement reader combining both legs
/// @param meas_set Reference to the dual measurement context
/// @param start_time_const Reference to the START leg's time correction constant
/// @param stop_time_const Reference to the STOP leg's time correction constant
void dualReaderCliBasic(
    DualMeasReader &dual_reader,
    const DualMeasContext &meas_set,
    const Measurement &start_time_const,
    const Measurement &stop_time_const);

#endif
