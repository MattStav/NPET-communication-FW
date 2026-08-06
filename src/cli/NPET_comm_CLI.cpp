#include "NPET_comm_CLI.h"

#include <conio.h>
#include <quadmath.h>
#include <spdlog/fmt/ranges.h>  // enables formatting of vectors, arrays, etc.

#include "meas_reader_CLI.h"
#include "ntp_sync.h"

constexpr std::string_view NO_PORTS = "No available COM ports found";
constexpr std::string_view INVALID_COM_PORT = "Invalid COM port number";
constexpr std::string_view FAILED_OPEN_COM_PORT = "Failed to open the COM port: {}";
constexpr std::string_view FAILED_OPEN_COM_PORT_MAX_ATTEMPT = "Failed to open NPET communication after {} attempts";
constexpr std::string_view NPET_OK_RESPONDING = "NPET communication is OK";
constexpr std::string_view NPET_NOT_RESPONDING = "NPET not responding!";
constexpr std::string_view FW_CURRENT = "Current NPET firmware version";
constexpr std::string_view FW_INVALID = "Invalid choice, firmware version not changed";
constexpr std::string_view FW_UNKNOWN = "Unknown firmware version detected";
constexpr std::string_view INVALID_NUM = "Invalid input. Number out of allowed range";
constexpr int INVALID_NUM_SENTINEL = -2;
constexpr std::string_view FREQ_ERR = "Couldn't set the frequency";
constexpr std::string_view FREQ_SET = "Pulse generation frequency set to [Hz]";
constexpr std::string_view FREQ_RESET_FAIL = "Failed to reset the pulse generation frequency";
constexpr std::string_view PULSE_GEN_OK = "Pulse generation successful";
constexpr std::string_view PULSE_GEN_ERR = "Pulse generation failed";
constexpr std::string_view PULSE_GEN_STOP_FAIL = "Failed to stop pulse generation";
constexpr std::string_view BAUD_RATE_CURRENT = "Current baud rate";
constexpr std::string_view BAUD_RATE_OK = "Baud rate set to";
constexpr std::string_view BAUD_RATE_ERR = "Failed to set baud rate";
constexpr std::string_view BAUD_RATE_RESET_FAIL = "Failed to reset baud rate to default";
constexpr std::string_view CHANNEL_INVALID = "Invalid channel number";
constexpr std::string_view DATA_FORMAT_RESET_FAIL = "Failed to reset the data format";
constexpr std::string_view MONITOR_FN_INVALID = "Invalid monitor function";
constexpr std::string_view TIME_CONST_FAILED_TO_SET = "Failed to define raw time constant: {}";
constexpr std::string_view TIME_CONST_FAILED_TO_CLEAR = "Failed to clear the time correction constant saved in NPET";
constexpr std::string_view TIME_CONST_FAILED_TO_EXPORT = "Failed to export the time constant to NPET";
constexpr std::string_view TIME_CONST_CLEAR_OK = "Time correction constant cleared";
constexpr std::string_view TIME_CONST_FRAC_MEAS_ERR = "Error occurred during fraction measurement";
constexpr std::string_view TIME_CONST_FRAC_INVALID_MEAS_NUM = "Invalid number of averaging measurements";
constexpr std::string_view TIME_CONST_FRAC_MEAS_INTERRUPT =
        "Fraction calibration interrupted by user. Fraction will be set to 0.00";
constexpr std::string_view TIME_CONST_FRAC_MEAS = "Measuring average fractional part of the time correction constant";
constexpr std::string_view TIME_CONST_INVALID = "Time correction constant invalid";
constexpr std::string_view TIME_CONST_SET = "New time correction constant set to";
constexpr std::string_view TIME_CONST_SET_FRAC_PART = "Invalid input - Fraction must be in the range (0, 1)";
constexpr std::string_view TIME_CONST_CURRENT = "Current time constant value";
constexpr std::string_view TIME_CONST_ADJUST = "Adjusting the time correction constant by [s]";
constexpr std::string_view SYSTEM_TIME_CURRENT = "Current system time is";
constexpr std::string_view RESET_INITIATED = "Resetting NPET to default settings";
constexpr std::string_view RESET_COMPLETE = "NPET reset sequence finished";

///
/// Lists available COM ports and prompts the user to select one.
/// If autoselect is true and only one COM port is available, it will be selected automatically.
/// WARNING: This function does not check if the selected COM port is valid.
/// @param AUTOSELECT If true, automatically select the COM port if only one is available.
/// @throws runtime_error if no COM ports are found.
/// @returns Selected COM port number (e.g. 8 for COM8).
static int selectComPortCli(const bool AUTOSELECT) {
    SPDLOG_DEBUG("Selecting COM port with autoselect: {}", AUTOSELECT);
    int selected_cp{};
    std::vector<std::string> com_ports{};

    // Get available com ports
    try {
        com_ports = getComPorts();
    } catch (std::runtime_error &e) {
        SPDLOG_ERROR(e.what());
        Cli::err(e.what());
    }
    // End the program if there are none
    if (com_ports.empty()) {
        SPDLOG_ERROR(NO_PORTS);
        Cli::err(std::string(std::string(NO_PORTS)) + "Make sure the NPET is connected and reset the program.");
        Cli::confirmExit();
        throw std::runtime_error("Error: No available COM ports found.");
    }
    // If there is only one COM port, select it automatically
    if (AUTOSELECT && com_ports.size() == 1) {
        SPDLOG_DEBUG("Autoselect enabled and only one COM port found: {}", com_ports.at(0));
        // Extract only the COM port number
        selected_cp = com_ports.at(0).c_str()[com_ports.at(0).size() - 3] - '0';
    } else {
        // Print available comports
        Cli::echo("--- Available COM ports ---", fg::gray, style::underline);
        for (const std::string &port: com_ports) {
            Cli::echo(port);
        }
        // Choose the COM port
        const std::string COM_PORT_STR = Cli::prompt("Choose COM port number");
        std::stringstream(COM_PORT_STR) >> selected_cp;
    }
    SPDLOG_INFO("Selected CP num: {}", selected_cp);
    return selected_cp;
} // end of select_COM_port function


///
/// Open serial communication with NPET
/// Initial detection of the COM port is attempted first, followed by user prompt if detection fails.
void NPETCommCLI::openCommunicationCLI() {
    constexpr int MAX_ATTEMPTS = 5;
    bool autoselect{true};

    SPDLOG_INFO("Opening NPET communication with CLI, max attempts: {}", MAX_ATTEMPTS);
    for (int i = 0; i < MAX_ATTEMPTS; i++) {
        // Limit the number of attempts to 5
        // After 2 failed attempts, disable autoselect
        if (i == 2) {
            autoselect = false;
        }
        SPDLOG_DEBUG("Attempt {} to open NPET communication, autoselect: {}", i + 1, autoselect);
        const int COM_PORT = selectComPortCli(autoselect);
        Cli::showInt("Opening the NPET communication on COM", COM_PORT);
        if (COM_PORT < 1) {
            SPDLOG_ERROR(INVALID_COM_PORT);
            Cli::err(std::string(INVALID_COM_PORT));
            continue;
        }
        try {
            openCommunication(COM_PORT, DEFAULT_BAUD_RATE);
        } catch (std::exception &e) {
            SPDLOG_ERROR(FAILED_OPEN_COM_PORT, e.what());
            Cli::err(std::format(FAILED_OPEN_COM_PORT, e.what()));
            continue;
        } // end of try-catch block
        if (!safeExec([&] { return isResponsive(); }, "is_responsive")) {
            Cli::echo("COM port opened successfully", fg::yellow);
            SPDLOG_ERROR(NPET_NOT_RESPONDING);
            Cli::err(std::string(NPET_NOT_RESPONDING));
            getPort().close();
            continue;
        }
        SPDLOG_INFO("NPET communication opened successfully");
        return;
    } // end of for loop
    SPDLOG_ERROR(FAILED_OPEN_COM_PORT_MAX_ATTEMPT, MAX_ATTEMPTS);
    Cli::err(std::format(FAILED_OPEN_COM_PORT_MAX_ATTEMPT, MAX_ATTEMPTS));
} // end of open_NPET_communication function


///
/// Checks if the NPET device is connected to the specified COM port and responsive.
/// Several attempts are made with a delay between each attempt.
/// Prints the status to the CLI.
/// @return True if NPET is connected and responsive, otherwise false.
bool NPETCommCLI::isResponsiveCLI() {
    constexpr int MAX_ATTEMPTS = 5;
    constexpr int RETRY_DELAY_MS = 1000;

    SPDLOG_DEBUG("Checking NPET responsiveness; max attempts: {}; retry delay: {} ms", MAX_ATTEMPTS, RETRY_DELAY_MS);
    for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
        SPDLOG_DEBUG("Responsiveness check attempt: {}", attempt + 1);
        if (safeExec([&] { return isResponsive(); }, "is_responsive")) {
            SPDLOG_DEBUG(NPET_OK_RESPONDING);
            std::cout << '\n'; // Empty line
            Cli::echo(std::string(NPET_OK_RESPONDING), fg::green);
            return true;
        }
        SPDLOG_ERROR(NPET_NOT_RESPONDING);
        Cli::err(std::string(NPET_NOT_RESPONDING));
        SPDLOG_DEBUG("Cancelling pending comms and purging all buffers before retrying ...");
        getPort().cancel();
        PurgeComm(getPort().native_handle(), PURGE_RXCLEAR | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_TXABORT);
        Sleep(RETRY_DELAY_MS);
    } // end of for loop
    SPDLOG_ERROR("NPET is not responsive after {} attempts", MAX_ATTEMPTS);
    return false;
} // end of is_NPET_connected_CLI function


///
/// Ask the user to select what version of FW the connected NPET device is running.
/// Firmware version is saved into the fw_version attribute.
void NPETCommCLI::setFwVerCLI() {
    const std::vector FW_OPTIONS = {
        std::string(FWVersion(FWVersion::ORIGINAL).getDescription()),
        std::string(FWVersion(FWVersion::AD_REVISION).getDescription()),
    };
    SPDLOG_DEBUG("{}: {}", FW_CURRENT, fw_version.getValue());
    Cli::showInt(std::string(FW_CURRENT), fw_version.getValue());
    SPDLOG_DEBUG("Selecting new version from: {}", FW_OPTIONS);
    const int USER_CHOICE = Cli::menu("What firmware is your NPET using?", FW_OPTIONS, false);
    if (USER_CHOICE == FWVersion::ORIGINAL || USER_CHOICE == FWVersion::AD_REVISION) {
        Cli::echo("Selected firmware: " + FW_OPTIONS.at(USER_CHOICE - 1));
    } else {
        SPDLOG_ERROR(FW_INVALID);
        Cli::err(std::string(FW_INVALID));
        return;
    }
    SPDLOG_DEBUG("Setting NPET firmware version to: {}", FW_OPTIONS.at(USER_CHOICE - 1).data());
    safeExec([&] { setFWVer(USER_CHOICE); }, "set_FW_ver");
} // end of select_NPET_FW_ver function


///
/// CLI wrapper for detect_FW_ver function.
void NPETCommCLI::detectFwVerCLI() {
    SPDLOG_DEBUG("Detecting NPET firmware version ...");
    safeExec([&] { detectFWVer(); }, "detect_FW_ver");
    try {
        const std::string DESCRIPTION = "Detected firmware: " + std::string(fw_version.getDescription());
        if (fw_version.getValue() == FWVersion::VIRTUAL) {
            Cli::echo(DESCRIPTION, fg::yellow);
        } else {
            Cli::echo(DESCRIPTION);
        }
    } catch (const std::invalid_argument &) {
        SPDLOG_ERROR(FW_UNKNOWN);
        throw std::invalid_argument(std::string(FW_UNKNOWN));
    }
} // end of detect_FW_ver_CLI function


///
/// Data validation allows either a positive integer number or (optionally) -1 for infinity.
/// @param num_to_validate Number to check as string
/// @param ALLOW_NEGATIVE_ONE Whether to allow -1 as a valid input
/// @return The validated number in int format, or -2 if the input is invalid
static int numValidation(const std::string &num_to_validate, const bool ALLOW_NEGATIVE_ONE = true) {
    int number_int = 0;
    SPDLOG_DEBUG("Validating number: {}; allow infinity sentinel: {}", num_to_validate, ALLOW_NEGATIVE_ONE);

    try {
        // Attempt to convert the string to an integer
        number_int = std::stoi(num_to_validate);
        if (const int MIN_VALID_VALUE = ALLOW_NEGATIVE_ONE ? -1 : 0; number_int < MIN_VALID_VALUE) {
            throw std::invalid_argument("Number is out of allowed range");
        }
    } catch (const std::invalid_argument &) {
        SPDLOG_ERROR(INVALID_NUM);
        Cli::err(std::string(INVALID_NUM));
        return INVALID_NUM_SENTINEL;
    }
    SPDLOG_DEBUG("Number {} is valid", number_int);
    return number_int;
} // end of is_valid_num_of_measurements_CLI function


///
/// CLI wrapper for the generate_pulses function.
/// Asks the user for the number of pulses to generate and the frequency.
void NPETCommCLI::generatePulsesCLI() {
    SPDLOG_DEBUG("Generating pulses ...");
    // Set the number of pulses
    const std::string NUM_OF_PULSES_STR = Cli::prompt("Enter number of pulses (-1 for inf; 0 to stop generation)", "0");
    const int NUM_OF_PULSES = numValidation(NUM_OF_PULSES_STR);
    if (NUM_OF_PULSES == INVALID_NUM_SENTINEL) {
        return; // Invalid input, return to the main menu
    }
    SPDLOG_DEBUG("User specified number for pulses: {}", NUM_OF_PULSES);
    // Set pulse generation frequency
    if (NUM_OF_PULSES != 0) {
        SPDLOG_DEBUG("Prompting user for pulse generation frequency ...");
        const std::string FREQUENCY_STR = Cli::prompt("Set pulse generation frequency [Hz] (0 to cancel)", "100");
        const int FREQUENCY_INT = numValidation(FREQUENCY_STR, false);
        if (FREQUENCY_INT == INVALID_NUM_SENTINEL || FREQUENCY_INT == 0) {
            return;
        }
        // Invalid input, return to the main menu
        SPDLOG_DEBUG("User specified pulse generation frequency: {} Hz", FREQUENCY_INT);
        if (!safeExec([&] { return setFrequency(FREQUENCY_INT); }, "set_frequency")) {
            SPDLOG_ERROR(FREQ_ERR);
            Cli::err(std::string(FREQ_ERR));
            return;
        }
        SPDLOG_DEBUG("{}: {}", FREQ_SET, FREQUENCY_INT);
        Cli::showInt(std::string(FREQ_SET), FREQUENCY_INT);
    }
    if (NUM_OF_PULSES == -1) {
        Cli::echo("Generating infinity pulses...");
    } else if (NUM_OF_PULSES == 0) {
        Cli::echo("Stopping pulse generation");
    } else {
        Cli::showInt("Generating pulses", NUM_OF_PULSES);
    }
    if (safeExec([&] { return generatePulses(NUM_OF_PULSES); }, "generate_pulses")) {
        SPDLOG_INFO(PULSE_GEN_OK);
        Cli::echo(std::string(PULSE_GEN_OK), fg::green);
    } else {
        SPDLOG_ERROR(PULSE_GEN_ERR);
        Cli::echo(std::string(PULSE_GEN_ERR), fg::red);
    }
} // end of generate_pulses_handler function


///
/// ClI wrapper for the set_baud_rate function.
void NPETCommCLI::setBaudRateCLI() {
    SPDLOG_DEBUG("Setting baud rate ...");
    int new_baud_rate{};
    const std::vector<std::string> BAUD_RATE_OPTIONS = {
        "115200",
        "230400",
        "576000",
    };
    SPDLOG_DEBUG("Possible new baud rates: {}", BAUD_RATE_OPTIONS);
    // Get current baud rate
    boost::asio::serial_port_base::baud_rate current_baud{};
    getPort().get_option(current_baud);
    SPDLOG_DEBUG("{}: {}", BAUD_RATE_CURRENT, current_baud.value());
    Cli::showInt(std::string(BAUD_RATE_CURRENT), static_cast<int>(current_baud.value()));
    switch (Cli::menu("New baud rate", BAUD_RATE_OPTIONS, false)) {
        case 1:
            new_baud_rate = 115200;
            break;
        case 2:
            new_baud_rate = 230400;
            break;
        case 3:
            new_baud_rate = 576000;
            break;
        default:
            return;
    }
    try {
        SPDLOG_WARN("INITIATING BAUD RATE CHANGE!");
        Cli::echo("YOU ARE ABOUT TO CHANGE THE COMMUNICATION BAUD RATE.", fg::yellow, style::bold);
        Cli::echo("DO NOT DISCONNECT THE DEVICE OR CLOSE THE PROGRAM!", fg::yellow, style::bold);
        Cli::echo("IF THIS PROCESS FAILS, RESTART THE DEVICE AND LAUNCH THIS PROGRAM ANEW.", fg::yellow, style::bold);
        Sleep(1000); // Wait for 1 second to let the user read the warning
        if (setBaudRate(new_baud_rate)) {
            SPDLOG_DEBUG("{}: {}", BAUD_RATE_OK, new_baud_rate);
            Cli::showInt(std::string(BAUD_RATE_OK), new_baud_rate);
        } else {
            SPDLOG_ERROR(BAUD_RATE_ERR);
            Cli::err(std::string(BAUD_RATE_ERR));
        }
    } catch (std::runtime_error &e) {
        SPDLOG_ERROR("{}: {}", BAUD_RATE_ERR, e.what());
        Cli::err(BAUD_RATE_ERR.data() + std::string(e.what()));
        SPDLOG_WARN("Attempting to reopen the communication");
        Cli::echo("Attempting to reopen the communication. If this keeps failing, reset the NPET!", fg::yellow);
        openCommunicationCLI();
    } // end of try-catch block
} // end of set_baud_rate_handler function


///
/// CLI wrapper for the read_measurements function.
/// Asks the user for the number of measurements, channel to read from, display, and save options
void NPETCommCLI::readBatchMeasurementsCLI() {
    SPDLOG_DEBUG("Reading batch measurements ...");
    // Prompt user for the number of measurements
    const std::string MEAS_NUM_STR = Cli::prompt("Number of measurements (-1 for inf; 0 to cancel)", "0");
    int num_of_meas = numValidation(MEAS_NUM_STR);
    if (num_of_meas == 0 || num_of_meas == INVALID_NUM_SENTINEL) {
        return;
    }
    if (num_of_meas == -1) {
        num_of_meas = INFINITE_OPERATION; // Magic number for infinite measurements
    }
    SPDLOG_DEBUG("User specified number of measurements to read: {}", num_of_meas);
    // Prompt user for channel number
    const std::string CHANNEL_STR = Cli::prompt("Select channel to read from (1 or 2; 0 to cancel)", "1");
    int channel{};
    try {
        channel = std::stoi(CHANNEL_STR);
        if (channel == 0) {
            return;
        }
        if (channel < 0 || channel >= 3) {
            throw std::invalid_argument(std::string(CHANNEL_INVALID));
        }
    } catch (const std::invalid_argument &) {
        SPDLOG_ERROR(CHANNEL_INVALID);
        Cli::err(std::string(CHANNEL_INVALID));
        return;
    } // end of try-catch block
    SPDLOG_DEBUG("User specified channel number: {}", channel);
    // Prompt user for the display and save options
    const std::string MONITOR_STR = Cli::prompt("Measurement monitoring (0 - None, 1 - Basic, 2 - Advanced, 3 - Sync)",
                                                "1");
    int monitor{};
    try {
        monitor = std::stoi(MONITOR_STR);
    } catch (const std::invalid_argument &) {
        SPDLOG_ERROR(MONITOR_FN_INVALID);
        Cli::err(std::string(MONITOR_FN_INVALID));
        return;
    } // end of try-catch block
    SPDLOG_DEBUG("User specified measurement monitoring: {}", monitor);
    std::function<void(MeasReader &, const MeasContext &, const Measurement &)>
            monitor_fn;
    switch (monitor) {
        case 1: monitor_fn = readerCliBasic;
            break;
        case 2: monitor_fn = readerCliAdvanced;
            break;
        case 3: monitor_fn = readerCliSync;
            break;
        default: monitor_fn = nullptr;
            break;
    }
    SPDLOG_DEBUG("Monitoring function successfully assigned");
    const bool SAVE_FLAG = Cli::confirm("Save the measurements?", true);
    SPDLOG_DEBUG("User specified save measurements flag: {}", SAVE_FLAG);
    // Prepare measurement context object
    const MeasContext MEAS_SETTINGS{
        .num_of_meas = num_of_meas,
        .monitor_fn = monitor_fn,
        .save_dir = SAVE_FLAG ? std::optional{USER_FILES} : std::nullopt,
        .channel = static_cast<Channel>(channel),
    };
    SPDLOG_DEBUG("Measurement context object: {}", MEAS_SETTINGS.toString());
    safeExec([&] { readBatchMeasurements(MEAS_SETTINGS); }, "read_batch_measurements");
    SPDLOG_INFO("Finished reading measurements");
} // end of read_measurements_handler function


///
/// Set the time correction constant on the NPET device.
/// User can choose between raw format input, time format input, or clearing the constant.
/// Either way, the time correction constant saved in the NPET and in the program is always the same.
/// In the end, the constant is exported to the NPET and sample measurements are read to show the effect.
void NPETCommCLI::setTimeConstantCLI() {
    SPDLOG_DEBUG("Setting time correction constant ...");
    Measurement new_const{.meas_num = -1};
    const std::vector<std::string> DEFINITION_OPTIONS = {
        "Raw format: int_sec frac_sec",
        "Time format: hh:mm:ss",
        "Clear time constant",
        "Cancel",
    };
    const std::vector<std::string> INT_OPTIONS = {
        "User defined",
        "System time",
        "System time with NTP sync (Admin required)",
        "Cancel",
    };

    SPDLOG_DEBUG("Possible new time correction constant definitions: {}", DEFINITION_OPTIONS);
    switch (Cli::menu("Time correction constant definition", DEFINITION_OPTIONS, false)) {
        case 1:
            SPDLOG_DEBUG("User selected raw format definition for time correction constant");
            try {
                new_const = rawTimeConstant();
            } catch (std::exception &e) {
                SPDLOG_ERROR(TIME_CONST_FAILED_TO_SET, e.what());
                Cli::err(std::format(TIME_CONST_FAILED_TO_SET.data(), e.what()));
                return;
            }
            break;
        case 2: {
            SPDLOG_DEBUG("User selected time format definition for time correction constant");
            const int AVER_NUM = std::stoi(Cli::prompt("Number of averaging measurements (>=2)", "16"));
            if (AVER_NUM < 2) {
                SPDLOG_ERROR(TIME_CONST_FRAC_INVALID_MEAS_NUM);
                Cli::err(std::string(TIME_CONST_FRAC_INVALID_MEAS_NUM));
                return;
            }
            SPDLOG_DEBUG("User specified number of measurements for averaging: {}", AVER_NUM);
            const Channel PPS_CHANNEL = static_cast<Channel>(std::stoi(
                Cli::prompt("What channel is the PPS connected to?", "2")));
            SPDLOG_DEBUG("User specified PPS channel: {}", static_cast<int>(PPS_CHANNEL));
            SPDLOG_DEBUG(TIME_CONST_FRAC_MEAS);
            Cli::echo(std::string(TIME_CONST_FRAC_MEAS));
            auto bar = ProgressBar({.total = AVER_NUM});
            const std::optional<__float128> AVE_FRAC = getAverageFraction(AVER_NUM, PPS_CHANNEL, &bar);
            Cli::echo(""); // New line after progress bar
            if (!AVE_FRAC.has_value()) {
                SPDLOG_ERROR(TIME_CONST_FRAC_MEAS_ERR);
                Cli::err(std::string(TIME_CONST_FAILED_TO_SET));
                SPDLOG_DEBUG("Breaking measurement stream ...");
                (void) isResponsive(true); // Break measurement stream
                return;
            }
            // Use the negative average fractional part to compensate for the offset
            new_const.fracp = -AVE_FRAC.value();
            SPDLOG_DEBUG("Measured average fractional part of the time correction constant: {}",
                         float128ToString(new_const.fracp));
            SPDLOG_DEBUG("Prompting user for integer part definition logic ...");
            SPDLOG_DEBUG("Possible integer part definition logic options: {}", INT_OPTIONS);
            const int INT_CHOICE = Cli::menu("Integer part setting logic", INT_OPTIONS, false);
            SPDLOG_DEBUG("User selected integer part definition logic: {}", INT_CHOICE);
            if (INT_CHOICE == 4) {
                return;
            }
            new_const.intp = calcInteger(static_cast<IntLogic>(INT_CHOICE), PPS_CHANNEL);
            SPDLOG_DEBUG("Calculated integer part of the time correction constant: {}", new_const.intp);
            break;
        }
        case 3:
            SPDLOG_DEBUG("User selected to clear the time correction constant");
            if (!safeExec([&] { return clearTimeConstant(); }, "clear_time_constant")) {
                SPDLOG_ERROR(TIME_CONST_FAILED_TO_CLEAR);
                Cli::err(std::string(TIME_CONST_FAILED_TO_CLEAR));
            } else {
                SPDLOG_INFO(TIME_CONST_CLEAR_OK);
                Cli::echo(std::string(TIME_CONST_CLEAR_OK), fg::green);
            }
            return;
        default:
            SPDLOG_DEBUG("User selected to cancel the time correction constant setting");
            return;
    } // end-of-switch
    // Measurement num -1 marks the time correction constant
    // If we don't have a valid time correction constant here, then exit
    if (new_const.meas_num != -1) {
        SPDLOG_ERROR(TIME_CONST_INVALID);
        Cli::err((std::string(TIME_CONST_INVALID)));
        return;
    }
    if (!safeExec([&] { return exportTimeConstant(new_const); }, "export_time_constant")) {
        SPDLOG_ERROR(TIME_CONST_FAILED_TO_EXPORT);
        Cli::err((std::string(TIME_CONST_FAILED_TO_EXPORT)));
        return;
    }
    SPDLOG_INFO("{}: {}", TIME_CONST_SET, new_const.toString());
    Cli::showStr(std::string(TIME_CONST_SET), new_const.toString());
    // Read sample measurements to see the results
    SPDLOG_DEBUG("Reading sample measurements to show the effect of the new time correction constant ...");
    safeExec([&] {
                 readBatchMeasurements(MeasContext{
                     .num_of_meas = 10, .monitor_fn = readerCliSync, .channel = Channel::CH2,
                 });
             },
             "read_batch_measurements");
} // end of set_time_constant_handler function


///
/// Ask the user to provide the time correction constant in raw format [int, frac].
/// It is also possible to adjust the existing value by a specified number of seconds.
/// @return The new time correction constant in measurement format.
Measurement NPETCommCLI::rawTimeConstant() {
    SPDLOG_DEBUG("Defining time correction constant in raw format ...");
    Measurement new_const{.meas_num = -1}; // Measurement num -1 marks the measurement as a time correction constant
    std::string input_string{};

    SPDLOG_DEBUG("Importing existing time correction constant from NPET ...");
    const Measurement OLD_CONST = safeExec([&] { return importTimeConstant(); }, "import_time_constant");
    SPDLOG_INFO("{}: {}", TIME_CONST_CURRENT, OLD_CONST.toString());
    Cli::showStr(std::string(TIME_CONST_CURRENT), OLD_CONST.toString());
    Cli::echo("Insert new time correction constant (or adjust the existing value)");
    // Either enter the new value of seconds or adjust the new one by +- value
    SPDLOG_DEBUG("Prompting user for new time correction constant or adjustment ...");
    input_string = Cli::prompt("Seconds (0 to cancel, +-value to adjust)", "0");
    SPDLOG_DEBUG("User input for time correction constant: {}", input_string);
    if (input_string == "0") {
        // Return an invalid measurement to indicate cancellation
        return Measurement{.meas_num = -2};
    }
    // Handle constant adjustment
    if (const char SIGN = input_string.at(0); SIGN == '+' || SIGN == '-') {
        SPDLOG_DEBUG("User input indicates an adjustment of the existing time correction constant");
        // Extract the number of seconds to adjust
        const int SECONDS_ADJUSTMENT = std::stoi(input_string.substr(1));
        // Adjust the constant_secs value
        SPDLOG_DEBUG("{}: {}{}", TIME_CONST_ADJUST, SIGN, SECONDS_ADJUSTMENT);
        Cli::showStr(std::string(TIME_CONST_ADJUST), input_string);
        new_const = OLD_CONST; // Copy the old constant
        if (SIGN == '+') {
            new_const.intp += SECONDS_ADJUSTMENT;
        } else {
            new_const.intp -= SECONDS_ADJUSTMENT;
        }
    } else {
        SPDLOG_DEBUG("User input indicates new time correction constant definition");
        // Handle new constant definition
        new_const.intp = std::stoi(input_string);
        SPDLOG_DEBUG("New constant int part: {}", new_const.intp);
        input_string = Cli::prompt("Fraction of a second (0.xxx format)", "0.0");
        const __float128 FRAC_PART = strtoflt128(input_string.c_str(), nullptr); // Convert input to float
        if (0 >= FRAC_PART || FRAC_PART >= 1) {
            SPDLOG_ERROR("{}: {}", TIME_CONST_SET_FRAC_PART, float128ToString(FRAC_PART));
            Cli::err(std::string(TIME_CONST_SET_FRAC_PART));
            return Measurement{.meas_num = -2};
        }
        new_const.fracp = FRAC_PART;
        SPDLOG_DEBUG("New constant fractional part: {}", float128ToString(new_const.fracp));
    } // end of if-else block
    SPDLOG_INFO("New time correction constant: {}", new_const.toString());
    return new_const;
} // end of raw_time_constant_CLI function


///
/// Define the integer part of the time correction constant.
/// @param INT_LOGIC Either ask the user to define the integer part of the time constant or grab system time
/// @param CHANNEL_NUM Channel number to read the measurements from (1 or 2). Only used if INT_LOGIC is
///        IntLogic::SYSTEM_TIME or IntLogic::NTP_SYNC.
/// @return The integer part of the time correction constant
int NPETCommCLI::calcInteger(const IntLogic INT_LOGIC, const Channel CHANNEL_NUM) {
    SPDLOG_DEBUG("Calculating integer part of the time correction constant with logic id: {}",
                 static_cast<int>(INT_LOGIC));
    int user_choice{};
    int clock_seconds{};

    switch (INT_LOGIC) {
        case IntLogic::MANUAL:
            SPDLOG_DEBUG("Logic: Define manually, user will be prompted to enter the target time ...");
            // User defined integer part
            Cli::echo("Enter time of the next 1 Hz measurement in hh:mm:ss format");
            Cli::echo(
                "You will be asked to confirm the values after inputting ss, the calibration will begin once you've confirmed.",
                fg::yellow);
            user_choice = std::stoi(Cli::prompt("Hours", "0"));
            clock_seconds = 3600 * user_choice;
            user_choice = std::stoi(Cli::prompt("Minutes", "0"));
            clock_seconds += 60 * user_choice;
            user_choice = std::stoi(Cli::prompt("Seconds", "0"));
            clock_seconds += user_choice;
            SPDLOG_DEBUG("User specified target time in seconds since midnight: {}", clock_seconds);
            Cli::showInt("Defined clock seconds", clock_seconds);
            SPDLOG_DEBUG("Awaiting final user confirmation for the calibration time");
            if (const bool CONFIRM = Cli::confirm(
                "Confirm by pressing `Enter` when the designated time is about to happen",
                true
            ); !CONFIRM) {
                // Cancel the calibration
                SPDLOG_ERROR("User cancelled the time correction constant integer part calibration");
                return 0;
            }
            SPDLOG_DEBUG(
                "Final confirmation received, time correction constant integer part will be set to the defined clock seconds");
            break;
        case IntLogic::NTP_SYNC:
            SPDLOG_DEBUG("Logic: Synchronize system time with NTP server ...");
            // Query an NTP server for the current time
            if (!ensureAccurateSystemTime()) {
                Cli::err("Failed to synchronize system time with NTP server");
            }
        // Intentional fallthrough to case IntLogic::SYSTEM_TIME
        case IntLogic::SYSTEM_TIME: {
            SPDLOG_DEBUG("Logic: Use system time ...");
            // Get the current system time
            Cli::echo("Calculating time correction integer constant from system time");
            const std::time_t NOW = std::time(nullptr);
            std::tm local_time{};
            localtime_s(&local_time, &NOW);
            std::ostringstream oss;
            oss << std::put_time(&local_time, "%H:%M:%S");
            SPDLOG_DEBUG("{}: {}", SYSTEM_TIME_CURRENT, oss.str());
            Cli::showStr(std::string(SYSTEM_TIME_CURRENT), oss.str());
            // Calculate seconds since midnight
            clock_seconds = (local_time.tm_hour * 3600) + (local_time.tm_min * 60) + local_time.tm_sec;
            SPDLOG_DEBUG("Calculated seconds since midnight: {}", clock_seconds);
            break;
        }
        default:
            SPDLOG_ERROR(INVALID_NUM);
            Cli::err(std::string(INVALID_NUM));
            return 0;
    } // end of switch
    // Get the current NPET time
    SPDLOG_DEBUG("Reading current measurement from channel {} to get the NPET time ...", static_cast<int>(CHANNEL_NUM));
    const Measurement CURRENT_MEASUREMENT = safeExec([&] { return readSingleMeasurement(CHANNEL_NUM); },
                                                     "read_single_measurement");
    SPDLOG_DEBUG("Current measurement read: {}", CURRENT_MEASUREMENT.toString());
    // Integer part of the time correction constant is the difference between the clock time and the measured time
    const int INTEGER_PART = clock_seconds - CURRENT_MEASUREMENT.intp;
    SPDLOG_DEBUG("Calculated integer part of the time correction constant: {}", INTEGER_PART);
    return INTEGER_PART;
} // end of calc_integer_CLI function


///
/// Reset NPET into default settings.
void NPETCommCLI::resetCLI() {
    SPDLOG_INFO(RESET_INITIATED);
    Cli::echo(std::string(RESET_INITIATED), fg::yellow);
    if (!safeExec([&] { return clearTimeConstant(); }, "clear_time_constant")) {
        SPDLOG_ERROR(TIME_CONST_FAILED_TO_CLEAR);
        Cli::err(std::string(TIME_CONST_FAILED_TO_CLEAR));
    }
    if (!safeExec([&] { return generatePulses(0); }, "generate_pulses")) {
        SPDLOG_ERROR(PULSE_GEN_STOP_FAIL);
        Cli::err(std::string(PULSE_GEN_STOP_FAIL));
    }
    if (!safeExec([&] { return setFrequency(); }, "set_frequency")) {
        SPDLOG_ERROR(FREQ_RESET_FAIL);
        Cli::err(std::string(FREQ_RESET_FAIL));
    }
    if (!safeExec([&] { return setMeasuredDataFormat(1); }, "set_measured_data_format")) {
        SPDLOG_ERROR(DATA_FORMAT_RESET_FAIL);
        Cli::err(std::string(DATA_FORMAT_RESET_FAIL));
    }
    if (!safeExec([&] { return setBaudRate(); }, "set_baud_rate")) {
        SPDLOG_ERROR(BAUD_RATE_RESET_FAIL);
        Cli::err(std::string(BAUD_RATE_RESET_FAIL));
    }
    SPDLOG_INFO(RESET_COMPLETE);
    Cli::echo(std::string(RESET_COMPLETE), fg::green);
} // end of reset_NPET function
