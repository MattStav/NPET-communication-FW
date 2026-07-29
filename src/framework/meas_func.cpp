#include "meas_func.h"

#include <array>
#include <cassert>
#include <ctime>
#include <quadmath.h>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>  // enables formatting of vectors, arrays, etc.

constexpr std::string_view INVALID_CHECKSUM_ERR = "Invalid checksum";
constexpr std::string_view INVALID_MEASUREMENT_ERR = "Invalid measurement data received from NPET: {}";


///
/// Convert a 128bit floating point number to string.
/// @param VALUE 128bit floating point number
/// @return String representation of the number
std::string float128ToString(const __float128 VALUE) {
    std::array<char, 256> buf{};
    // Fixed decimal with the correct quad precision rounding
    quadmath_snprintf(buf.data(), buf.size(), FMT, VALUE); // NOLINT(cppcoreguidelines-pro-type-vararg)
    return buf.data();
} // end of float128_to_string function


///
/// Generate a name for the output file.
/// The name includes the NPET channel and a datetime stamp to ensure uniqueness.
/// @param CHANNEL NPET channel number (1 or 2)
/// @param base_dir Base directory to save the output file, defaults to the current working directory
/// @return Output file name
std::string outputFilePath(const int CHANNEL, const std::filesystem::path &base_dir) {
    assert(CHANNEL == 1 || CHANNEL == 2); // Check that the channel number is valid
    SPDLOG_DEBUG("Generating output file name for channel {}", CHANNEL);
    // Get the current time
    const std::time_t NOW = std::time(nullptr);
    std::tm local_time{};
    localtime_s(&local_time, &NOW);
    // Format the timestamp as YYYYMMDD_HHMMSS
    std::array<char, 20> timestamp{};
    std::strftime(timestamp.data(), timestamp.size(), "%Y%m%d_%H%M%S", &local_time);
    SPDLOG_DEBUG("Current timestamp for file name: {}", timestamp.data());
    const std::filesystem::path OUTPUT_DIR = base_dir / OUTPUT_DIR_NAME;
    SPDLOG_DEBUG("Output directory path: {}", OUTPUT_DIR.string());
    // Create the output directory if it doesn't exist
    if (!std::filesystem::exists(OUTPUT_DIR)) {
        std::filesystem::create_directories(OUTPUT_DIR);
    }
    // Construct the filename
    const std::filesystem::path FILE = OUTPUT_DIR / ("EPOCH" + std::to_string(CHANNEL) + "_" + timestamp.data() + ".out");
    SPDLOG_DEBUG("Output file name: {}", FILE.string());
    return FILE.string();
} // end of output_file_name function


///
/// Calculate the XOR checksum of the first 12 bytes of the given 13-byte set
/// @param set_to_check Set of bytes to get the checksum for
/// @return The computed checksum byte
uint8_t xorChecksum(const std::array<std::uint8_t, 13> &set_to_check) {
    std::uint8_t computed = 0;

    // XOR first 12 bytes: indices [0..11]
    for (std::size_t i = 0; i < 12; ++i) {
        computed ^= set_to_check.at(i);
    }
    SPDLOG_DEBUG("Array: {}, Computed checksum: {:02X}, Received checksum: {:02X}",
                 set_to_check,
                 computed,
                 set_to_check.at(12)
    );
    return computed;
} // end of xorChecksum function


///
/// Process the data received from NPET.
/// Compute the time of photon arrival from the measured data.
/// Corrects the result with the time correction constant from NPET.
/// Uses 128-bit floating point numbers to avoid overflow.
/// @param MEASUREMENT_SET Array of 11 measured data received from NPET in binary format.
/// The data received from NPET has 13 bytes, but the first two bytes are not used in the computation.
/// @param multiplier Multiplier depending on NPET FW version
/// @param time_const Time correction constant, defaults to empty const
Measurement decodeMeasurementSet(
    const std::array<std::uint8_t, 13> MEASUREMENT_SET,
    const __float128 &multiplier,
    const Measurement &time_const
) {
    assert(time_const.meas_num == -1); // Check that the time constant is correctly marked with -1
    __float128 temp_int_holder{};
    Measurement result{};

    if (xorChecksum(MEASUREMENT_SET) != MEASUREMENT_SET.at(12)) {
        SPDLOG_ERROR(INVALID_CHECKSUM_ERR);
        throw std::runtime_error(std::string(INVALID_CHECKSUM_ERR));
    }
    if (MEASUREMENT_SET.at(0) != 1 || MEASUREMENT_SET.at(1) != 11) {
        // Check docu for explanation
        SPDLOG_ERROR(INVALID_MEASUREMENT_ERR, MEASUREMENT_SET);
        throw std::invalid_argument(std::format(INVALID_MEASUREMENT_ERR, MEASUREMENT_SET));
    }
    result.meas_num = static_cast<int>(MEASUREMENT_SET.at(2));
    const __float128 MEASURED_VALUE = ((MEASUREMENT_SET.at(3) + (MEASUREMENT_SET.at(4) * powq(2, 8)) +
                                      (MEASUREMENT_SET.at(5) * powq(2, 16))) * powq(2, 24) * multiplier) +
                                      ((MEASUREMENT_SET.at(6) + (MEASUREMENT_SET.at(7) * powq(2, 8)) +
                                      (MEASUREMENT_SET.at(8) * powq(2, 16))) * multiplier) +
                                      ((MEASUREMENT_SET.at(9) + (MEASUREMENT_SET.at(10) * powq(2, 8)) +
                                      (MEASUREMENT_SET.at(11) * powq(2, 16)) - powq(2, 22) +
                                      static_cast<__float128>(1)) / powq(2, 16) * static_cast<__float128>(0.00000001));
    result.fracp = modfq(MEASURED_VALUE, &temp_int_holder); // Take the fractional part of the measured value
    result.intp = static_cast<int>(temp_int_holder); // Take the integer part of the measured value
    // Add the time correction constant
    result += time_const;
    // Handle fraction overflow into seconds
    result.resolve();
    SPDLOG_DEBUG("Processed measurement: {}, Time constant: {}, Result: {}",
                 MEASUREMENT_SET,
                 time_const.toString(),
                 result.toString()
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
/// measurement.intp/fracp mirror measurement::intp/fracp exactly, so a decoded measurement can be passed straight
/// back in to reproduce (as closely as the format allows) the bytes it came from.
/// @param measurement Target arrival time to encode: intp is the whole-second part (must be non-negative), fracp
/// is the fractional-second part (must be in [0, 1)), and meas_num is embedded in byte 2 (becomes
/// measurement::meas_num on decode)
/// @param multiplier Multiplier depending on NPET FW version; must match what decode_measurement_set is later called with
/// @return A 13-byte measurement set, checksum included, that decodes back to approximately the requested time
std::array<std::uint8_t, 13> encodeMeasurementSet(
    const Measurement &measurement,
    const __float128 &multiplier
) {
    constexpr long long FINE_ZERO_POINT = 4'194'303LL; // -2^22 + 1, undoes the offset decode_measurement_set applies
    constexpr long long MAX_24_BIT = 0xFFFFFF;
    constexpr long long MAX_COMBINED = (1ULL << 48U) - 1; // coarse (24 bits) and medium (24 bits) packed together
    const int SECONDS = measurement.intp;
    const __float128 &fracp = measurement.fracp;
    if (SECONDS < 0) {
        throw std::invalid_argument("seconds must be non-negative");
    }
    if (fracp < 0 || fracp >= 1) {
        throw std::invalid_argument("fracp must be in [0, 1)");
    }
    // The fine term's LSB: 1e-8 / 2^16 seconds, always - decode_measurement_set never scales it by multiplier
    const __float128 FINE_LSB = powq(10, -8) / powq(2, 16);
    const __float128 TARGET_TIME = static_cast<__float128>(SECONDS) + fracp;
    // Snap to the nearest point on the multiplier-sized grid; this is the coarse+medium fields, packed as one
    // 48-bit value (coarse holds the top 24 bits, medium the bottom 24), since coarse's LSB is exactly 2^24 * medium's.
    const long long COMBINED = llroundq(TARGET_TIME / multiplier);
    if (COMBINED < 0 || COMBINED > MAX_COMBINED) {
        throw std::out_of_range("Requested time is outside the representable range");
    }
    const auto COMBINED_BITS = static_cast<unsigned long long>(COMBINED);
    const auto COARSE = static_cast<std::uint32_t>(COMBINED_BITS >> 24U);
    const auto MEDIUM = static_cast<std::uint32_t>(COMBINED_BITS & static_cast<unsigned long long>(MAX_24_BIT));
    // Whatever the coarse/medium grid couldn't capture is encoded by the fine correction term
    const __float128 REMAINDER = TARGET_TIME - (static_cast<__float128>(COMBINED) * multiplier);
    const long long FINE_RAW = llroundq(REMAINDER / FINE_LSB) + FINE_ZERO_POINT;
    if (FINE_RAW < 0 || FINE_RAW > MAX_24_BIT) {
        throw std::out_of_range("Requested time is outside the representable range");
    }
    const auto FINE_BITS = static_cast<unsigned long long>(FINE_RAW);
    // Combine everything together
    std::array<std::uint8_t, 13> measurement_set{};
    measurement_set.at(0) = 1;
    measurement_set.at(1) = 11;
    measurement_set.at(2) = static_cast<std::uint8_t>(measurement.meas_num);
    measurement_set.at(3) = static_cast<std::uint8_t>(COARSE);
    measurement_set.at(4) = static_cast<std::uint8_t>(COARSE >> 8U);
    measurement_set.at(5) = static_cast<std::uint8_t>(COARSE >> 16U);
    measurement_set.at(6) = static_cast<std::uint8_t>(MEDIUM);
    measurement_set.at(7) = static_cast<std::uint8_t>(MEDIUM >> 8U);
    measurement_set.at(8) = static_cast<std::uint8_t>(MEDIUM >> 16U);
    measurement_set.at(9) = static_cast<std::uint8_t>(FINE_BITS);
    measurement_set.at(10) = static_cast<std::uint8_t>(FINE_BITS >> 8U);
    measurement_set.at(11) = static_cast<std::uint8_t>(FINE_BITS >> 16U);
    measurement_set.at(12) = xorChecksum(measurement_set);
    SPDLOG_DEBUG("Encoded target time {}.{} s into measurement set: {}",
                 SECONDS, float128ToString(fracp), measurement_set);
    return measurement_set;
} // end of encode_measurement_set function


///
/// Formats the measurement command string to be sent to the NPET device.
/// @param CHANNEL NPET channel to read from (1 or 2)
/// @param NUM_OF_MEAS Number of measurements to read
/// @return Formatted measurement command string
std::string getMeasurementCmd(const Channel CHANNEL, const int NUM_OF_MEAS) {
    const std::string MEAS_LETTER = CHANNEL == Channel::CH1 ? "e" : "h";
    const std::string CMD = MEAS_LETTER + std::to_string(NUM_OF_MEAS);
    SPDLOG_DEBUG("Measurement command: {}", CMD);
    return CMD;
} // end of get_measurement_command function


///
/// Get the measurement multiplier based on NPET firmware version.
/// @param FW_VERSION NPET firmware version (1, 2, or 3)
/// @return Measurement multiplier as a 128-bit floating point number
__float128 getMeasurementMultiplier(const int FW_VERSION) {
    __float128 mult{};
    if (FW_VERSION == 1) {
        mult = 0.00000002;
    } else if (FW_VERSION == 2 || FW_VERSION == 3) {
        mult = 0.00000001;
    } else {
        throw std::invalid_argument("Unknown FW version");
    }
    SPDLOG_DEBUG("Measurement multiplier for FW version {}: {}", FW_VERSION, float128ToString(mult));
    return mult;
} // end of get_measurement_multiplier function
