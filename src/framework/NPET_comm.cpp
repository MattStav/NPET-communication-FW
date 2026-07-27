#include "NPET_comm.h"

#include <boost/asio.hpp>
#include <string>
#include <fstream>
#include <quadmath.h>
#include <spdlog/spdlog.h>
#ifdef PYBIND11_ENABLED
#include <pybind11/gil.h>
#endif

#include "meas_reader.h"

constexpr std::string_view DATA_FORMAT_ERR = "Failed to set proper measured data format before reading measurements";
constexpr std::string_view SLEEP_DISABLE_ERR = "Failed to disable Windows sleep while measurements are active";
constexpr std::string_view SLEEP_ENABLE_ERR = "Failed to re-enable windows sleep settings";


///
/// Checks if the NPET device is connected and responsive.
/// @return: True, indicating the device is responsive. Otherwise, return false.
bool NPET_comm::is_responsive(const bool end_stream) {
    SPDLOG_DEBUG("Checking if NPET is responsive");
    const std::string response = exchange_comm("c");
    // The NPET responds with "c0" to the "c" command.
    // In case the NPET is currently streaming measurements,
    // it responds with "c1" instead, which will be at the end of the response string.
    const bool responsive = response == "c0" || (end_stream && response.ends_with("c1"));
    SPDLOG_INFO("NPET responsiveness check: {}, Response: '{:?}'", responsive ? "Responsive" : "Not responsive",
                response);
    return responsive;
} // end of is_responsive function


///
/// Set the firmware version.
/// Firmware version must be either 1, 2, or 3.
/// Version is saved into the fw_version attribute.
/// @param new_fw_version Firmware version to set
void NPET_comm::set_FW_ver(const int new_fw_version) {
    SPDLOG_DEBUG("Setting NPET firmware version to {}", new_fw_version);
    assert((new_fw_version == 1 || new_fw_version == 2 || new_fw_version == 3) && "Invalid NPET firmware version");
    fw_version = new_fw_version;
    SPDLOG_INFO("NPET firmware successfully version set to {}", fw_version);
} // end of set_NPET_FW_ver function


///
/// Automatically detect the NPET firmware version by querying the device.
/// The firmware version is saved into the fw_version attribute.
void NPET_comm::detect_FW_ver() {
    SPDLOG_DEBUG("Detecting NPET firmware version");
    // Strings to find
    const std::string revision_string = "ADI";
    const std::string offline_string = "offline";
    // Get and check the FW version
    if (const std::string res = exchange_comm("?"); res.find(revision_string) != std::string::npos) {
        // Set the FW version to 2 running NPET with the latest components revision
        set_FW_ver(2);
    } else if (res.find(offline_string) != std::string::npos) {
        // Set the FW version to 3 if running with virtual NPET
        set_FW_ver(3);
    } else {
        // Set the FW version to 1 if running the original NPET
        set_FW_ver(1);
    }
} // end of automatic_FW_detection function


///
/// Set the pulse generation frequency on the NPET device.
/// The Default frequency on NPET startup is 100 Hz.
/// Save the new frequency into the frequency attribute.
/// @param new_frequency New pulse generation frequency in Hz
/// @return True if the frequency was successfully set, otherwise false
bool NPET_comm::set_frequency(const int new_frequency) {
    SPDLOG_DEBUG("Setting pulse generation frequency to {} Hz", new_frequency);
    assert(new_frequency >= 1);
    const std::string ret = exchange_comm("k" + std::to_string(new_frequency));
    const bool success = ret.starts_with('k');
    success
        ? SPDLOG_INFO("Pulse generation frequency successfully set to {} Hz", new_frequency)
        : SPDLOG_ERROR("Failed to set pulse generation frequency to {} Hz", new_frequency);
    return success;
} // end of set_frequency function


///
/// Command the NPET device to generate a specified number of pulses.
/// @param num_of_pulses Number of pulses to generate, -1 for infinite
/// @return True if the command was successful, otherwise false
bool NPET_comm::generate_pulses(const int num_of_pulses) {
    std::string log_num = num_of_pulses == -1 ? "infinite" : std::to_string(num_of_pulses);
    SPDLOG_DEBUG("Generating {} pulses from NPET", log_num);
    assert(num_of_pulses >= 0 || num_of_pulses == -1);
    std::string num_of_pulses_str{};
    if (num_of_pulses == -1) num_of_pulses_str = std::to_string(INFINITE_OPERATION);
    else num_of_pulses_str = std::to_string(num_of_pulses);
    const std::string ret = exchange_comm("p" + num_of_pulses_str);
    const bool success = ret.starts_with('p');
    success
        ? SPDLOG_INFO("Pulse generation command successful for {} pulses", log_num)
        : SPDLOG_ERROR("Pulse generation command failed for {} pulses", log_num);
    return success;
} // end of generate_pulses function


///
/// Set the port baud rate.
/// The default baud rate on NPET startup is 115_200.
/// This operation cannot use the exchange_comm framework, making it very brittle.
/// DO NOT DISCONNECT THE DEVICE WHILE CHANGING THE BAUD RATE.
/// @param new_baud_rate New baud rate to set
/// @return True if the baud rate was successfully changed, otherwise false
bool NPET_comm::set_baud_rate(const int new_baud_rate) {
    SPDLOG_DEBUG("Setting baud rate to {}", new_baud_rate);
    assert(new_baud_rate > 0);
    // Cancel any pending operations before changing the baud rate
    SPDLOG_DEBUG("Cancelling pending operations");
    get_port().cancel();
    // THIS FUNCTION CANNOT USE SEND_COMMAND FROM THIS MODULE!!!
    const std::string cmd = "w" + std::to_string(new_baud_rate);
    write_to_serial(cmd);
    // Read response to clear the buffer
    read_from_serial();
    get_port().set_option(boost::asio::serial_port_base::baud_rate(new_baud_rate));
    const bool success = is_responsive();
    success
        ? SPDLOG_INFO("Baud rate successfully set to {}", new_baud_rate)
        : SPDLOG_ERROR("Failed to set baud rate to {}", new_baud_rate);
    return success;
} // end of set_baud_rate function


///
/// Set the measured data format on the NPET device.
/// @param format Measured data format. 0 for binary, 1 for ASCII
/// @return True if the format was successfully set, otherwise false
bool NPET_comm::set_measured_data_format(const int format) {
    std::string log_format = format == 0 ? "binary" : "ASCII";
    SPDLOG_DEBUG("Setting measured data format to {}", log_format);
    assert(format == 0 || format == 1);
    const std::string ret = exchange_comm("a" + std::to_string(format));
    const bool success = ret.starts_with('a');
    success
        ? SPDLOG_INFO("Measured data format successfully set to {}", log_format)
        : SPDLOG_ERROR("Failed to set measured data format to {}", log_format);
    return success;
} // end of set_measured_data_format function


///
/// Use the measurement_reader object to read measurements from the NPET device.
/// Measurements are read in binary format.
/// Windows sleep is disabled while the measurements are being read.
/// @param meas_set Measurement context struct
/// /// Contains the number of measurements, display and save flags, and channel number
void NPET_comm::read_batch_measurements(const meas_context &meas_set) {
    SPDLOG_DEBUG("Reading batch measurements from NPET");
    assert(meas_set.num_of_meas > 0);
    assert(meas_set.channel == 1 || meas_set.channel == 2);
    // Set the measured data format to binary
    // This program can only process the binary data format
    if (!set_measured_data_format(0)) {
        SPDLOG_ERROR(DATA_FORMAT_ERR);
        throw std::runtime_error(DATA_FORMAT_ERR.data());
    }
    // Release the GIL to allow other threads to run while reading measurements
#ifdef PYBIND11_ENABLED
    pybind11::gil_scoped_release release;
#endif
    // Disable system sleep while this thread runs (Windows specific)
    if (SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED) == 0) {
        SPDLOG_ERROR(SLEEP_DISABLE_ERR);
        throw std::runtime_error(SLEEP_DISABLE_ERR.data());
    }
    // Call the measurement_reader
    [[maybe_unused]] meas_reader session(*this, meas_set);
    // Re-enable system sleep after the critical function completes
    if (SetThreadExecutionState(ES_CONTINUOUS) == 0) {
        SPDLOG_ERROR(SLEEP_ENABLE_ERR);
        throw std::runtime_error(SLEEP_ENABLE_ERR.data());
    }
    SPDLOG_INFO("Batch measurements reading completed");
} // end of read_measurements function


///
/// Read a single measurement from the specified channel.
/// @param channel Channel to read from (1 or 2)
/// @return Single measurement from the specified channel
measurement NPET_comm::read_single_measurement(const int channel) {
    SPDLOG_DEBUG("Reading single measurement from NPET");
    assert(channel == 1 || channel == 2);
    std::vector<char> vec{};
    std::array<uint8_t, MEASUREMENT_PACKET_SIZE> arr{};

    // Set the measured data format to binary.
    // This program can only process the binary data format
    if (!set_measured_data_format(0)) {
        SPDLOG_ERROR(DATA_FORMAT_ERR);
        throw std::runtime_error(DATA_FORMAT_ERR.data());
    }
    write_to_serial(get_measurement_cmd(channel, 1));
    vec = read_with_timeout(ReadMode::FixedBytes, 5000, MEASUREMENT_PACKET_SIZE);
    SPDLOG_DEBUG("Single measurement received");  // Logging the data is pointless as it's unformatted
    // Transform the binary response into a measurement array
    std::transform(vec.begin(), vec.begin() + MEASUREMENT_PACKET_SIZE, arr.begin(),
                   [](const char c) { return static_cast<uint8_t>(c); });
    return process_measurement(arr, get_measurement_multiplier(fw_version));
} // end of read_single_measurement function


///
/// Read a single measurement from the specified channel in raw string format, without termination.
/// @param channel Channel to read from (1 or 2)
/// @return Single measurement from the specified channel in raw string format, without termination.
/// /// Empty string if no measurement was read.
std::string NPET_comm::read_single_measurement_raw(const int channel) {
    SPDLOG_DEBUG("Reading single raw measurement from NPET");
    assert(channel == 1 || channel == 2);
    const measurement meas = read_single_measurement(channel);
    if (meas.is_empty()) return "";
    SPDLOG_DEBUG("Raw measurement data received: '{}'", meas.to_string());
    return meas.to_string();
} // end of read_single_measurement_raw function


///
/// Export the time constant to the NPET device.
/// @param constant Time constant in measurement format
/// @return True if the time constant was successfully exported, otherwise false.
bool NPET_comm::export_time_constant(const measurement &constant) {
    SPDLOG_DEBUG("Exporting time constant to NPET: '{}'", constant.to_string());
    assert(!constant.is_empty());
    const bool success = export_time_constant_raw(constant.to_string());
    success
        ? SPDLOG_INFO("Time constant successfully exported to NPET: {}", constant.to_string())
        : SPDLOG_ERROR("Failed to export time constant to NPET: {}", constant.to_string());
    return success;
} // end of export_time_constant function


///
/// Export the time constant to the NPET device.
/// @param constant_raw Time constant in string format, without termination!
/// Can be a maximum of 28 characters long.
/// @return True if the time constant was successfully exported, otherwise false.
bool NPET_comm::export_time_constant_raw(const std::string &constant_raw) {
    SPDLOG_DEBUG("Exporting raw time constant to NPET: '{}'", constant_raw);
    SPDLOG_WARN("This will overwrite the previous time correction constant!");
    assert(constant_raw.length() <= 28);
    const std::string ret = exchange_comm("j" + constant_raw);
    const bool success = ret.starts_with('j');
    success
        ? SPDLOG_INFO("Raw time constant successfully exported to NPET: '{}'", constant_raw)
        : SPDLOG_ERROR("Failed to export raw time constant to NPET: '{}'", constant_raw);
    return success;
} // end of export_time_constant_raw function


///
/// Clear the time constant saved in the NPET devic.
/// @return True if the time constant was successfully cleared on the NPET, otherwise false.
bool NPET_comm::clear_time_constant() {
    SPDLOG_DEBUG("Clearing time constant from NPET");
    const std::string EMPTY_CONSTANT(28, ' '); // 28 spaces
    const bool success = export_time_constant_raw(EMPTY_CONSTANT);
    success
        ? SPDLOG_INFO("Time constant successfully cleared from NPET")
        : SPDLOG_ERROR("Failed to clear time constant from NPET");
    return success;
} // end of clear_time_constant_on_NPET function


///
/// Import the time correction constant from the NPET device.
/// This should be the only way to get the time constant within the program!!
/// Ensuring that the value saved in NPET and in the program are always the same.
/// @return Measurement object containing the time constant.
measurement NPET_comm::import_time_constant() {
    SPDLOG_DEBUG("Importing time constant from NPET");
    // Send command to get the time constant
    std::string raw_const = exchange_comm("n1");
    SPDLOG_DEBUG("Raw time constant received from NPET: '{:?}'", raw_const);
    // Data validation, triggered if no constant was returned from the NPET
    // Either got no response, which is suspicious for other reasons,
    // or the third (fifth when there's \r\n) char is empty
    if (raw_const.empty() || raw_const.length() < 3 || raw_const[4] == ' ') {
        SPDLOG_INFO("No time constant found on NPET, returning empty constant");
        return measurement{-1};
    }
    // Remove the start of the string upto the first \n (included)
    raw_const = raw_const.substr(raw_const.find('\n') + 1);
    // Remove the end of the string from the first \n (included)
    raw_const = raw_const.substr(0, raw_const.find('\n'));
    const std::string int_part = raw_const.substr(0, raw_const.find(' '));
    const std::string frac_part = raw_const.substr(raw_const.find(' ') + 1);
    // Convert the raw_const to digits
    const measurement constant = {
        -1,
        std::stoi(int_part),
        strtoflt128(frac_part.c_str(), nullptr)
    };
    SPDLOG_INFO("Processed time constant: {}", constant.to_string());
    return constant;
} // end of import_time_constant_from_NPET function


///
/// Import the time correction constant from the NPET device in raw string format, without termination.
/// @return Time constant imported from the NPET device in raw string format, without termination.
std::string NPET_comm::import_time_constant_raw() {
    SPDLOG_DEBUG("Importing raw time constant from NPET");
    const measurement constant = import_time_constant();
    if (constant.is_empty()) return "";
    SPDLOG_INFO("Raw time constant received from NPET: '{}'", constant.to_string());
    return constant.to_string();
} // end of import_time_constant_raw function


///
/// Get the status of the NPET device.
/// This command is only of use when NPET is set in measurement streaming mode, but it is NOT receiving any data.
/// @return Current status of the NPET device, check docu for more details.
std::string NPET_comm::get_status() {
    SPDLOG_DEBUG("Getting status from NPET");
    const std::string ret = exchange_comm("s1");
    SPDLOG_DEBUG("Status received from NPET: '{}'", ret);
    return ret;
} // end of get_status function
