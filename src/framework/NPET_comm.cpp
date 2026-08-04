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
bool NPETComm::isResponsive(const bool END_STREAM) {
    SPDLOG_DEBUG("Checking if NPET is responsive");
    const std::string RESPONSE = exchangeComm("c");
    // The NPET responds with "c0" to the "c" command.
    // In case the NPET is currently streaming measurements,
    // it responds with "c1" instead, which will be at the end of the response string.
    const bool RESPONSIVE = RESPONSE == "c0" || (END_STREAM && RESPONSE.ends_with("c1"));
    SPDLOG_INFO("NPET responsiveness check: {}, Response: '{:?}'", RESPONSIVE ? "Responsive" : "Not responsive",
                RESPONSE);
    return RESPONSIVE;
} // end of isResponsive function


///
/// Set the firmware version.
/// Firmware version must be either 1, 2, or 3.
/// Version is saved into the fw_version attribute.
/// @param NEW_FW_VERSION New firmware version to set
void NPETComm::setFWVer(const int NEW_FW_VERSION) {
    SPDLOG_DEBUG("Setting NPET firmware version to {}", NEW_FW_VERSION);
    assert((NEW_FW_VERSION == 1 || NEW_FW_VERSION == 2 || NEW_FW_VERSION == 3) && "Invalid NPET firmware version");
    fw_version = NEW_FW_VERSION;
    SPDLOG_INFO("NPET firmware successfully version set to {}", fw_version);
} // end of set_NPET_FW_ver function


///
/// Automatically detect the NPET firmware version by querying the device.
/// The firmware version is saved into the fw_version attribute.
void NPETComm::detectFWVer() {
    SPDLOG_DEBUG("Detecting NPET firmware version");
    // Strings to find
    const std::string REVISION_STRING = "ADI";
    const std::string OFFLINE_STRING = "offline";
    // Get and check the FW version
    if (const std::string RES = exchangeComm("?"); RES.contains(REVISION_STRING)) { // NOLINT(*-branch-clone)
        // Set the FW version to 2 running NPET with the latest components revision
        setFWVer(2);
    } else if (RES.contains(OFFLINE_STRING)) {
        // Set the FW version to 3 if running with virtual NPET
        setFWVer(3);
    } else {
        // Set the FW version to 1 if running the original NPET
        setFWVer(1);
    }
} // end of automatic_FW_detection function


///
/// Set the pulse generation frequency on the NPET device.
/// The Default frequency on NPET startup is 100 Hz.
/// Save the new frequency into the frequency attribute.
/// @param NEW_FREQUENCY New pulse generation frequency in Hz
/// @return True if the frequency was successfully set, otherwise false
bool NPETComm::setFrequency(const int NEW_FREQUENCY) {
    SPDLOG_DEBUG("Setting pulse generation frequency to {} Hz", NEW_FREQUENCY);
    assert(NEW_FREQUENCY >= 1);
    const std::string RET = exchangeComm("k" + std::to_string(NEW_FREQUENCY));
    const bool SUCCESS = RET.starts_with('k');
    SUCCESS
        ? SPDLOG_INFO("Pulse generation frequency successfully set to {} Hz", NEW_FREQUENCY)
        : SPDLOG_ERROR("Failed to set pulse generation frequency to {} Hz", NEW_FREQUENCY);
    return SUCCESS;
} // end of set_frequency function


///
/// Command the NPET device to generate a specified number of pulses.
/// @param NUM_OF_PULSES Number of pulses to generate, -1 for infinite
/// @return True if the command was successful, otherwise false
bool NPETComm::generatePulses(const int NUM_OF_PULSES) {
    std::string log_num = NUM_OF_PULSES == -1 ? "infinite" : std::to_string(NUM_OF_PULSES);
    SPDLOG_DEBUG("Generating {} pulses from NPET", log_num);
    assert(NUM_OF_PULSES >= 0 || NUM_OF_PULSES == -1);
    std::string num_of_pulses_str{};
    if (NUM_OF_PULSES == -1) {
        num_of_pulses_str = std::to_string(INFINITE_OPERATION);
    } else {
        num_of_pulses_str = std::to_string(NUM_OF_PULSES);
    }
    const std::string RET = exchangeComm("p" + num_of_pulses_str);
    const bool SUCCESS = RET.starts_with('p');
    SUCCESS
        ? SPDLOG_INFO("Pulse generation command successful for {} pulses", log_num)
        : SPDLOG_ERROR("Pulse generation command failed for {} pulses", log_num);
    return SUCCESS;
} // end of generate_pulses function


///
/// Set the port baud rate.
/// The default baud rate on NPET startup is 115_200.
/// This operation cannot use the exchange_comm framework, making it very brittle.
/// DO NOT DISCONNECT THE DEVICE WHILE CHANGING THE BAUD RATE.
/// @param NEW_BAUD_RATE New baud rate to set
/// @return True if the baud rate was successfully changed, otherwise false
bool NPETComm::setBaudRate(const int NEW_BAUD_RATE) {
    SPDLOG_DEBUG("Setting baud rate to {}", NEW_BAUD_RATE);
    assert(NEW_BAUD_RATE > 0);
    // Cancel any pending operations before changing the baud rate
    SPDLOG_DEBUG("Cancelling pending operations");
    getPort().cancel();
    // THIS FUNCTION CANNOT USE SEND_COMMAND FROM THIS MODULE!!!
    const std::string CMD = "w" + std::to_string(NEW_BAUD_RATE);
    writeToSerial(CMD);
    // Read response to clear the buffer
    readFromSerial();
    getPort().set_option(boost::asio::serial_port_base::baud_rate(NEW_BAUD_RATE));
    const bool SUCCESS = isResponsive();
    SUCCESS
        ? SPDLOG_INFO("Baud rate successfully set to {}", NEW_BAUD_RATE)
        : SPDLOG_ERROR("Failed to set baud rate to {}", NEW_BAUD_RATE);
    return SUCCESS;
} // end of set_baud_rate function


///
/// Set the measured data format on the NPET device.
/// @param FORMAT Measured data format. 0 for binary, 1 for ASCII
/// @return True if the format was successfully set, otherwise false
bool NPETComm::setMeasuredDataFormat(const int FORMAT) {
    std::string log_format = FORMAT == 0 ? "binary" : "ASCII";
    SPDLOG_DEBUG("Setting measured data format to {}", log_format);
    assert(FORMAT == 0 || FORMAT == 1);
    const std::string RET = exchangeComm("a" + std::to_string(FORMAT));
    const bool SUCCESS = RET.starts_with('a');
    SUCCESS
        ? SPDLOG_INFO("Measured data format successfully set to {}", log_format)
        : SPDLOG_ERROR("Failed to set measured data format to {}", log_format);
    return SUCCESS;
} // end of set_measured_data_format function


///
/// Use the measurement_reader object to read measurements from the NPET device.
/// Measurements are read in binary format.
/// Windows sleep is disabled while the measurements are being read.
/// @param meas_set Measurement context struct
/// /// Contains the number of measurements, display and save flags, and channel number
void NPETComm::readBatchMeasurements(const MeasContext &meas_set) {
    SPDLOG_DEBUG("Reading batch measurements from NPET");
    assert(meas_set.num_of_meas > 0);
    assert(meas_set.channel == 1 || meas_set.channel == 2);
    // Set the measured data format to binary
    // This program can only process the binary data format
    if (!setMeasuredDataFormat(0)) {
        SPDLOG_ERROR(DATA_FORMAT_ERR);
        throw std::runtime_error(std::string(DATA_FORMAT_ERR));
    }
    // Release the GIL to allow other threads to run while reading measurements
#ifdef PYBIND11_ENABLED
    pybind11::gil_scoped_release release;
#endif
    // Disable system sleep while this thread runs (Windows specific)
    if (SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED) == 0) {
        SPDLOG_ERROR(SLEEP_DISABLE_ERR);
        throw std::runtime_error(std::string(SLEEP_DISABLE_ERR));
    }
    // Call the measurement_reader
    [[maybe_unused]] MeasReader const SESSION(*this, meas_set);
    // Re-enable system sleep after the critical function completes
    if (SetThreadExecutionState(ES_CONTINUOUS) == 0) {
        SPDLOG_ERROR(SLEEP_ENABLE_ERR);
        throw std::runtime_error(std::string(SLEEP_ENABLE_ERR));
    }
    SPDLOG_INFO("Batch measurements reading completed");
} // end of readBatchMeasurements function


///
/// Read a single measurement from the specified channel.
/// @param CHANNEL Channel to read from (1 or 2)
/// @return Single measurement from the specified channel
Measurement NPETComm::readSingleMeasurement(const int CHANNEL) {
    SPDLOG_DEBUG("Reading single measurement from NPET");
    assert(CHANNEL == 1 || CHANNEL == 2);
    std::vector<char> vec{};
    std::array<uint8_t, MEASUREMENT_PACKET_SIZE> arr{};

    // Set the measured data format to binary.
    // This program can only process the binary data format
    if (!setMeasuredDataFormat(0)) {
        SPDLOG_ERROR(DATA_FORMAT_ERR);
        throw std::runtime_error(std::string(DATA_FORMAT_ERR));
    }
    writeToSerial(getMeasurementCmd(static_cast<Channel>(CHANNEL), 1));
    vec = readWithTimeout(ReadMode::FIXED_BYTES, 5000, MEASUREMENT_PACKET_SIZE);
    SPDLOG_DEBUG("Single measurement received"); // Logging the data is pointless as it's unformatted
    // Transform the binary response into a measurement array
    std::transform(vec.begin(), vec.begin() + MEASUREMENT_PACKET_SIZE, arr.begin(),
                   [](const char C) { return static_cast<uint8_t>(C); });
    return decodeMeasurementSet(arr, getMeasurementMultiplier(fw_version));
} // end of read_single_measurement function


///
/// Read a single measurement from the specified channel in raw string format, without termination.
/// @param CHANNEL Channel to read from (1 or 2)
/// @return Single measurement from the specified channel in raw string format, without termination.
/// /// Empty string if no measurement was read.
std::string NPETComm::readSingleMeasurementRaw(const int CHANNEL) {
    SPDLOG_DEBUG("Reading single raw measurement from NPET");
    assert(CHANNEL == 1 || CHANNEL == 2);
    const Measurement MEAS = readSingleMeasurement(CHANNEL);
    if (MEAS.isEmpty()) {
        return "";
    }
    SPDLOG_DEBUG("Raw measurement data received: '{}'", MEAS.toString());
    return MEAS.toString();
} // end of readSingleMeasurementRaw function


///
/// Export the time constant to the NPET device.
/// @param constant Time constant in measurement format
/// @return True if the time constant was successfully exported, otherwise false.
bool NPETComm::exportTimeConstant(const Measurement &constant) {
    SPDLOG_DEBUG("Exporting time constant to NPET: '{}'", constant.toString());
    assert(!constant.isEmpty());
    const bool SUCCESS = exportTimeConstantRaw(constant.toString());
    SUCCESS
        ? SPDLOG_INFO("Time constant successfully exported to NPET: {}", constant.toString())
        : SPDLOG_ERROR("Failed to export time constant to NPET: {}", constant.toString());
    return SUCCESS;
} // end of exportTimeConstant function


///
/// Export the time constant to the NPET device.
/// @param constant_raw Time constant in string format, without termination!
/// Can be a maximum of 28 characters long.
/// @return True if the time constant was successfully exported, otherwise false.
bool NPETComm::exportTimeConstantRaw(const std::string &constant_raw) {
    SPDLOG_DEBUG("Exporting raw time constant to NPET: '{}'", constant_raw);
    SPDLOG_WARN("This will overwrite the previous time correction constant!");
    assert(constant_raw.length() <= 28);
    const std::string RET = exchangeComm("j" + constant_raw);
    const bool SUCCESS = RET.starts_with('j');
    SUCCESS
        ? SPDLOG_INFO("Raw time constant successfully exported to NPET: '{}'", constant_raw)
        : SPDLOG_ERROR("Failed to export raw time constant to NPET: '{}'", constant_raw);
    return SUCCESS;
} // end of export_time_constant_raw function


///
/// Clear the time constant saved in the NPET devic.
/// @return True if the time constant was successfully cleared on the NPET, otherwise false.
bool NPETComm::clearTimeConstant() {
    SPDLOG_DEBUG("Clearing time constant from NPET");
    const std::string EMPTY_CONSTANT(28, ' '); // 28 spaces
    const bool SUCCESS = exportTimeConstantRaw(EMPTY_CONSTANT);
    SUCCESS
        ? SPDLOG_INFO("Time constant successfully cleared from NPET")
        : SPDLOG_ERROR("Failed to clear time constant from NPET");
    return SUCCESS;
} // end of clearTimeConstant function


///
/// Import the time correction constant from the NPET device.
/// This should be the only way to get the time constant within the program!!
/// Ensuring that the value saved in NPET and in the program are always the same.
/// @return Measurement object containing the time constant.
Measurement NPETComm::importTimeConstant() {
    SPDLOG_DEBUG("Importing time constant from NPET");
    // Send command to get the time constant
    std::string raw_const = exchangeComm("n1");
    SPDLOG_DEBUG("Raw time constant received from NPET: '{:?}'", raw_const);
    // Data validation, triggered if no constant was returned from the NPET
    // Either got no response, which is suspicious for other reasons,
    // or the third (fifth when there's \r\n) char is empty
    if (raw_const.empty() || raw_const.length() < 3 || raw_const.at(4) == ' ') {
        SPDLOG_INFO("No time constant found on NPET, returning empty constant");
        return Measurement{.meas_num = -1};
    }
    // Remove the start of the string upto the first \n (included)
    raw_const = raw_const.substr(raw_const.find('\n') + 1);
    // Remove the end of the string from the first \n (included)
    raw_const = raw_const.substr(0, raw_const.find('\n'));
    const std::string INT_PART = raw_const.substr(0, raw_const.find(' '));
    const std::string FRAC_PART = raw_const.substr(raw_const.find(' ') + 1);
    // Convert the raw_const to digits
    const Measurement CONSTANT = {
        .meas_num = -1,
        .intp = std::stoi(INT_PART),
        .fracp = strtoflt128(FRAC_PART.c_str(), nullptr),
    };
    SPDLOG_INFO("Processed time constant: {}", CONSTANT.toString());
    return CONSTANT;
} // end of importTimeConstant function


///
/// Import the time correction constant from the NPET device in raw string format, without termination.
/// @return Time constant imported from the NPET device in raw string format, without termination.
std::string NPETComm::importTimeConstantRaw() {
    SPDLOG_DEBUG("Importing raw time constant from NPET");
    const Measurement CONSTANT = importTimeConstant();
    if (CONSTANT.isEmpty()) {
        return "";
    }
    SPDLOG_INFO("Raw time constant received from NPET: '{}'", CONSTANT.toString());
    return CONSTANT.toString();
} // end of import_time_constant_raw function


///
/// Get the status of the NPET device.
/// This command is only of use when NPET is set in measurement streaming mode, but it is NOT receiving any data.
/// @return Current status of the NPET device, check docu for more details.
std::string NPETComm::getStatus() {
    SPDLOG_DEBUG("Getting status from NPET");
    const std::string RET = exchangeComm("s1");
    SPDLOG_DEBUG("Status received from NPET: '{}'", RET);
    return RET;
} // end of get_status function
