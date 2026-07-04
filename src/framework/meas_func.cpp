#include "meas_func.h"

#include <cassert>
#include <ctime>
#include <quadmath.h>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>  // enables formatting of vectors, arrays, etc.

constexpr std::string_view INVALID_CHECKSUM_ERR = "Invalid checksum";
constexpr std::string_view INVALID_MEASUREMENT_ERR = "Invalid measurement data received from NPET: {}";


///
/// Convert a 128bit floating point number to string.
/// @param value 128bit floating point number
/// @return String representation of the number
std::string float128_to_string(const __float128 value) {
    char buf[256];
    // Fixed decimal with the correct quad precision rounding
    quadmath_snprintf(buf, sizeof(buf), FMT, value);
    return buf;
} // end of float128_to_string function


///
/// Generate a name for the output file.
/// The name includes the NPET channel and a datetime stamp to ensure uniqueness.
/// @param channel Current NPET channel
/// @param base_dir Base directory to save the output file, defaults to the current working directory
/// @return Output file name
std::string output_file_path(const int channel, const std::filesystem::path &base_dir) {
    assert(channel == 1 || channel == 2); // Check that the channel number is valid
    SPDLOG_DEBUG("Generating output file name for channel {}", channel);
    // Get the current time
    const std::time_t now = std::time(nullptr);
    const std::tm *local_time = std::localtime(&now);
    // Format the timestamp as YYYYMMDD_HHMMSS
    char timestamp[20];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", local_time);
    SPDLOG_DEBUG("Current timestamp for file name: {}", timestamp);
    const std::filesystem::path output_dir = base_dir / OUTPUT_DIR_NAME;
    SPDLOG_DEBUG("Output directory path: {}", output_dir.string());
    // Create the output directory if it doesn't exist
    if (!std::filesystem::exists(output_dir)) {
        std::filesystem::create_directories(output_dir);
    }
    // Construct the filename
    const std::filesystem::path file = output_dir / ("EPOCH" + std::to_string(channel) + "_" + timestamp + ".out");
    SPDLOG_DEBUG("Output file name: {}", file.string());
    return file.string();
} // end of output_file_name function


///
/// Calculate the XOR checksum of the first 12 bytes of the given 13-byte set
/// @param set_to_check Set of bytes to get the checksum for
/// @return The computed checksum byte
uint8_t xor_checksum(const std::array<std::uint8_t, 13> &set_to_check) {
    std::uint8_t computed = 0;

    // XOR first 12 bytes: indices [0..11]
    for (std::size_t i = 0; i < 12; ++i) {
        computed ^= set_to_check[i];
    }
    SPDLOG_DEBUG("Array: {}, Computed checksum: {:02X}, Received checksum: {:02X}",
                  set_to_check,
                  computed,
                  set_to_check[12]
    );
    return computed;
} // end of xor_checksum_ok function


///
/// Process the data received from NPET.
/// Compute the time of photon arrival from the measured data.
/// Corrects the result with the time correction constant from NPET.
/// Uses 128-bit floating point numbers to avoid overflow.
/// @param measurement_set Array of 11 measured data received from NPET in binary format.
/// The data received from NPET has 13 bytes, but the first two bytes are not used in the computation.
/// @param multiplier Multiplier depending on NPET FW version
/// @param time_const Time correction constant, defaults to empty const
measurement process_measurement(
    const std::array<std::uint8_t, 13> measurement_set,
    const __float128 &multiplier,
    const measurement &time_const
) {
    assert(time_const.meas_num == -1); // Check that the time constant is correctly marked with -1
    __float128 temp_int_holder{};
    measurement result{};

    if (xor_checksum(measurement_set) != measurement_set[12]) {
        SPDLOG_ERROR(INVALID_CHECKSUM_ERR);
        throw std::runtime_error(INVALID_CHECKSUM_ERR.data());
    }
    if (measurement_set[0] != 1 || measurement_set[1] != 11) {
        // Check docu for explanation
        SPDLOG_ERROR(INVALID_MEASUREMENT_ERR, measurement_set);
        throw std::invalid_argument(std::format(INVALID_MEASUREMENT_ERR, measurement_set));
    }
    result.meas_num = static_cast<int>(measurement_set[2]);
    const __float128 measured_value = (measurement_set[3] + measurement_set[4] * powq(2, 8) + measurement_set[5] *
                                       powq(2, 16)) * powq(2, 24) * multiplier + (
                                          measurement_set[6] + measurement_set[7] * powq(2, 8) + measurement_set[8] *
                                          powq(2, 16)) * multiplier
                                      + (measurement_set[9] + measurement_set[10] * powq(2, 8) + measurement_set[11] *
                                         powq(2, 16) - powq(2, 22) + static_cast<__float128>(1)) / powq(2, 16) *
                                      static_cast<__float128>(0.00000001);
    result.fracp = modfq(measured_value, &temp_int_holder); // Take the fractional part of the measured value
    result.intp = static_cast<int>(temp_int_holder); // Take the integer part of the measured value
    // Add the time correction constant
    result += time_const;
    // Handle fraction overflow into seconds
    result.resolve();
    SPDLOG_DEBUG("Processed measurement: {}, Time constant: {}, Result: {}",
                  measurement_set,
                  time_const.to_string(),
                  result.to_string()
    );
    return result;
} // end of compute_time_of_arrival function


///
/// Formats the measurement command string to be sent to the NPET device.
/// @param channel NPET channel to read from (1 or 2)
/// @param num_of_meas Number of measurements to read
/// @return Formatted measurement command string
std::string get_measurement_cmd(const int channel, const int num_of_meas) {
    const std::string meas_letter = channel == 1 ? "e" : "h";
    const std::string cmd = meas_letter + std::to_string(num_of_meas);
    SPDLOG_DEBUG("Measurement command: {}", cmd);
    return cmd + "\r\n";
} // end of get_measurement_command function


///
/// Get the measurement multiplier based on NPET firmware version.
/// @param fw_version NPET firmware version
/// @return Measurement multiplier as a 128-bit floating point number
__float128 get_measurement_multiplier(const int fw_version) {
    __float128 mult{};
    if (fw_version == 1) mult = 0.00000002;
    else if (fw_version == 2 || fw_version == 3) mult = 0.00000001;
    else throw std::invalid_argument("Unknown FW version");
    SPDLOG_DEBUG("Measurement multiplier for FW version {}: {}", fw_version, float128_to_string(mult));
    return mult;
} // end of get_measurement_multiplier function
