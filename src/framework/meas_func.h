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

std::string outputFilePath(Channel CHANNEL, const std::filesystem::path &base_dir = USER_FILES,
                           const std::string &FILE_PREFIX = "");

uint8_t xorChecksum(const std::array<std::uint8_t, 13> &set_to_check);

std::string getMeasurementCmd(Channel CHANNEL, int NUM_OF_MEAS);

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

Measurement decodeMeasurementSet(std::array<std::uint8_t, 13> MEASUREMENT_SET,
                                const __float128 &multiplier,
                                const Measurement &time_const = Measurement{.meas_num = -1});

std::array<std::uint8_t, 13> encodeMeasurementSet(const Measurement &measurement, const __float128 &multiplier);

#endif //MEAS_FUNC_H
