#ifndef MEAS_FUNC_H
#define MEAS_FUNC_H
#include <cstdint> // Needed for uint8_t
#include <filesystem>
#include <string>
#include <quadmath.h>

#include "helper_func.h"

// Path to output directory
constexpr std::string OUTPUT_DIR_NAME = "FW_outputs";
// The number defines decimal precision for meas fractional part
constexpr auto FMT = "%.15Qf";

enum class Channel : std::uint8_t { CH1 = 1, CH2 = 2 };

///
/// Generate a name for the output file.
/// The name includes the NPET channel and a datetime stamp to ensure uniqueness.
/// @param CHANNEL NPET channel number (1 or 2)
/// @param base_dir Base directory to save the output file, defaults to the current working directory
/// @param FILE_PREFIX Optional prefix prepended to the filename (e.g. to distinguish NPETDual's two legs)
/// @return Output file name
std::string outputFilePath(Channel CHANNEL, const std::filesystem::path &base_dir = USER_FILES,
                           const std::string &FILE_PREFIX = "");

///
/// Calculate the XOR checksum of the first 12 bytes of the given 13-byte set
/// @param set_to_check Set of bytes to get the checksum for
/// @return The computed checksum byte
uint8_t xorChecksum(const std::array<std::uint8_t, 13> &set_to_check);

///
/// Formats the measurement command string to be sent to the NPET device.
/// @param CHANNEL NPET channel to read from (1 or 2)
/// @param NUM_OF_MEAS Number of measurements to read
/// @return Formatted measurement command string
std::string getMeasurementCmd(Channel CHANNEL, int NUM_OF_MEAS);

///
/// Convert a 128bit floating point number to string.
/// @param VALUE 128bit floating point number
/// @return String representation of the number
std::string float128ToString(__float128 VALUE);

///
/// Struct to hold measurement data with integer and fractional parts
struct Measurement {
    int meas_num{0}; // Time constants have -1
    int intp{0};
    __float128 fracp{0.0};

    [[nodiscard]] std::string toString() const {
        return std::to_string(intp) + " " + float128ToString(fracp);
    } // end of to_string function

    ///
    /// Check if the measurement is invalid, which is indicated by meas_num being -2.
    /// @return True if the measurement is valid, else False
    [[nodiscard]] bool isValid() const {
        return meas_num != -2;
    } // end of is_valid function

    [[nodiscard]] bool isEmpty() const {
        return intp == 0 && fracp == 0.0;
    } // end of is_empty function

    ///
    /// Resolve the measurement by handling fractional overflow into seconds.
    void resolve() {
        if (fracp < 0) {
            intp--;
            fracp++;
        } else if (fracp >= 1) {
            intp++;
            fracp--;
        }
    } // end of resolve function

    ///
    /// Round the measurement to the nearest integer, after resolving fractional overflow.
    /// @return The measurement rounded to the nearest integer, after resolving fractional overflow.
    [[nodiscard]] int round() const { return static_cast<int>(llroundq(intp + fracp)); }

    ///
    /// Sum operation of 2 measurements.
    /// The measurement_num of the resulting measurement is taken from the first operand.
    Measurement operator+(const Measurement &other) const {
        return {.meas_num = meas_num, .intp = intp + other.intp, .fracp = fracp + other.fracp};
    } // end of operator+ function

    Measurement &operator+=(const Measurement &other) {
        intp += other.intp;
        fracp += other.fracp;
        return *this;
    } // end of operator+= function
}; // end of correction_holder struct

///
/// Process the data received from NPET.
/// Compute the time of photon arrival from the measured data.
/// Corrects the result with the time correction constant from NPET.
/// Uses 128-bit floating point numbers to avoid overflow.
/// @param MEASUREMENT_SET Array of 11 measured data received from NPET in binary format.
/// The data received from NPET has 13 bytes, but the first two bytes are not used in the computation.
/// @param multiplier Multiplier depending on NPET FW version
/// @param time_const Time correction constant, defaults to empty const
Measurement decodeMeasurementSet(std::array<std::uint8_t, 13> MEASUREMENT_SET,
                                const __float128 &multiplier,
                                const Measurement &time_const = Measurement{.meas_num = -1});

///
/// Encode a target arrival time into the raw 13-byte format decoded by decode_measurement_set.
/// This is the inverse of decode_measurement_set (when called with the default, zero time_const): the coarse,
/// medium and fine fields are chosen so decoding the returned bytes reproduces the requested time as closely as
/// the wire format allows. Its finest representable step is 1e-8 / 2^16 s (~152.588 fs), fixed regardless of
/// multiplier, so femtosecond-precision input is rounded to the nearest representable tick - anything finer than
/// that is lost to quantization, the same way it would be on a real device.
/// measurement.intp/fracp mirror measurement::intp/fracp exactly, so a decoded measurement can be passed straight
/// back in to reproduce (as closely as the format allows) the bytes it came from.
/// @param measurement Target arrival time to encode: intp is the whole-second part (must be non-negative), fracp
/// is the fractional-second part (must be in [0, 1)), and meas_num is embedded in byte 2 (becomes
/// measurement::meas_num on decode)
/// @param multiplier Multiplier depending on NPET FW version; must match what decode_measurement_set is later called with
/// @return A 13-byte measurement set, checksum included, that decodes back to approximately the requested time
std::array<std::uint8_t, 13> encodeMeasurementSet(const Measurement &measurement, const __float128 &multiplier);

#endif //MEAS_FUNC_H
