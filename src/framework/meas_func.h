#ifndef MEAS_FUNC_H
#define MEAS_FUNC_H
#include <cstdint> // Needed for uint8_t
#include <filesystem>
#include <string>
#include <quadmath.h>

// Path to output directory
constexpr std::string OUTPUT_DIR_NAME = "NPET_output";
// The number defines decimal precision for meas fractional part
constexpr char FMT[] = "%.15Qf";

std::string output_file_path(int channel, const std::filesystem::path &base_dir = std::filesystem::current_path());

uint8_t xor_checksum(const std::array<std::uint8_t, 13> &set_to_check);

std::string get_measurement_cmd(int channel, int num_of_meas);

__float128 get_measurement_multiplier(int fw_version);

std::string float128_to_string(__float128 value);

///
/// Struct to hold measurement data with integer and fractional parts
struct measurement {
    int meas_num{0}; // Time constants have -1
    int intp{0};
    __float128 fracp{0.0};

    [[nodiscard]] std::string to_string() const {
        return std::to_string(intp) + " " + float128_to_string(fracp);
    } // end of to_string function

    ///
    /// Check if the measurement is invalid, which is indicated by meas_num being -2.
    /// @return True if the measurement is valid, else False
    [[nodiscard]] bool is_valid() const {
        return meas_num != -2;
    } // end of is_valid function

    [[nodiscard]] bool is_empty() const {
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
    measurement operator+(const measurement &other) const {
        return {meas_num, intp + other.intp, fracp + other.fracp};
    } // end of operator+ function

    measurement &operator+=(const measurement &other) {
        intp += other.intp;
        fracp += other.fracp;
        return *this;
    } // end of operator+= function
}; // end of correction_holder struct

measurement process_measurement(std::array<std::uint8_t, 13> measurement_set,
                                const __float128 &multiplier,
                                const measurement &time_const = measurement{-1});

#endif //MEAS_FUNC_H
