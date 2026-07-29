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
measurement decode_measurement_set(
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
/// Encode a target arrival time into the raw 13-byte format decoded by decode_measurement_set.
/// This is the inverse of decode_measurement_set (when called with the default, zero time_const): the coarse,
/// medium and fine fields are chosen so decoding the returned bytes reproduces the requested time as closely as
/// the wire format allows. Its finest representable step is 1e-8 / 2^16 s (~152.588 fs), fixed regardless of
/// multiplier, so femtosecond-precision input is rounded to the nearest representable tick - anything finer than
/// that is lost to quantization, the same way it would be on a real device.
/// seconds/fracp mirror measurement::intp/fracp exactly, so a decoded measurement's fields can be passed straight
/// back in to reproduce (as closely as the format allows) the bytes it came from.
/// @param meas_num Measurement number to embed in byte 2 (becomes measurement::meas_num on decode)
/// @param seconds Whole-second part of the target arrival time, must be non-negative
/// @param fracp Fractional-second part of the target arrival time, in [0, 1)
/// @param multiplier Multiplier depending on NPET FW version; must match what decode_measurement_set is later called with
/// @return A 13-byte measurement set, checksum included, that decodes back to approximately the requested time
std::array<std::uint8_t, 13> encode_measurement_set(
    const std::uint8_t meas_num,
    const int seconds,
    const __float128 &fracp,
    const __float128 &multiplier
) {
    constexpr long long FINE_ZERO_POINT = 4'194'303LL; // -2^22 + 1, undoes the offset decode_measurement_set applies
    constexpr long long MAX_24_BIT = 0xFFFFFF;
    constexpr long long MAX_COMBINED = (1LL << 48) - 1; // coarse (24 bits) and medium (24 bits) packed together
    if (seconds < 0) throw std::invalid_argument("seconds must be non-negative");
    if (fracp < 0 || fracp >= 1) throw std::invalid_argument("fracp must be in [0, 1)");
    // The fine term's LSB: 1e-8 / 2^16 seconds, always - decode_measurement_set never scales it by multiplier
    const __float128 fine_lsb = powq(10, -8) / powq(2, 16);
    const __float128 target_time = static_cast<__float128>(seconds) + fracp;
    // Snap to the nearest point on the multiplier-sized grid; this is the coarse+medium fields, packed as one
    // 48-bit value (coarse holds the top 24 bits, medium the bottom 24), since coarse's LSB is exactly 2^24 * medium's.
    const long long combined = llroundq(target_time / multiplier);
    if (combined < 0 || combined > MAX_COMBINED)
        throw std::out_of_range("Requested time is outside the representable range");
    const auto coarse = static_cast<std::uint32_t>(combined >> 24);
    const auto medium = static_cast<std::uint32_t>(combined & MAX_24_BIT);
    // Whatever the coarse/medium grid couldn't capture is encoded by the fine correction term
    const __float128 remainder = target_time - static_cast<__float128>(combined) * multiplier;
    const long long fine_raw = llroundq(remainder / fine_lsb) + FINE_ZERO_POINT;
    if (fine_raw < 0 || fine_raw > MAX_24_BIT)
        throw std::out_of_range("Requested time is outside the representable range");
    // Combine everything together
    std::array<std::uint8_t, 13> measurement_set{};
    measurement_set[0] = 1;
    measurement_set[1] = 11;
    measurement_set[2] = meas_num;
    measurement_set[3] = static_cast<std::uint8_t>(coarse);
    measurement_set[4] = static_cast<std::uint8_t>(coarse >> 8);
    measurement_set[5] = static_cast<std::uint8_t>(coarse >> 16);
    measurement_set[6] = static_cast<std::uint8_t>(medium);
    measurement_set[7] = static_cast<std::uint8_t>(medium >> 8);
    measurement_set[8] = static_cast<std::uint8_t>(medium >> 16);
    measurement_set[9] = static_cast<std::uint8_t>(fine_raw);
    measurement_set[10] = static_cast<std::uint8_t>(fine_raw >> 8);
    measurement_set[11] = static_cast<std::uint8_t>(fine_raw >> 16);
    measurement_set[12] = xor_checksum(measurement_set);
    SPDLOG_DEBUG("Encoded target time {}.{} s into measurement set: {}",
                 seconds, float128_to_string(fracp), measurement_set);
    return measurement_set;
} // end of encode_measurement_set function


///
/// Formats the measurement command string to be sent to the NPET device.
/// @param channel NPET channel to read from (1 or 2)
/// @param num_of_meas Number of measurements to read
/// @return Formatted measurement command string
std::string get_measurement_cmd(const int channel, const int num_of_meas) {
    const std::string meas_letter = channel == 1 ? "e" : "h";
    const std::string cmd = meas_letter + std::to_string(num_of_meas);
    SPDLOG_DEBUG("Measurement command: {}", cmd);
    return cmd;
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
