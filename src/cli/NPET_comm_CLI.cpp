#include "NPET_comm_CLI.h"

#include <conio.h>
#include <quadmath.h>
#include <spdlog/fmt/ranges.h>  // enables formatting of vectors, arrays, etc.

#include "helper_func.h"
#include "measurement_reader_CLI.h"
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
constexpr std::string_view TIME_CONST_FRAC_MEAS_INTERRUPT = "Fraction calibration interrupted by user. Fraction will be set to 0.00";
constexpr std::string_view TIME_CONST_FRAC_MEAS = "Measuring average fractional part of the time correction constant";
constexpr std::string_view TIME_CONST_INVALID = "Time correction constant invalid";
constexpr std::string_view TIME_CONST_SET = "New time correction constant set to";
constexpr std::string_view TIME_CONST_SET_FRAC_PART ="Invalid input - Fraction must be in the range (0, 1)";
constexpr std::string_view TIME_CONST_CURRENT = "Current time constant value";
constexpr std::string_view TIME_CONST_ADJUST = "Adjusting the time correction constant by [s]";
constexpr std::string_view SYSTEM_TIME_CURRENT = "Current system time is";
constexpr std::string_view RESET_INITIATED = "Resetting NPET to default settings";
constexpr std::string_view RESET_COMPLETE = "NPET reset sequence finished";

///
/// Lists available COM ports and prompts the user to select one.
/// If autoselect is true and only one COM port is available, it will be selected automatically.
/// WARNING: This function does not check if the selected COM port is valid.
/// @param autoselect If true, automatically select the COM port if only one is available.
/// @throws runtime_error if no COM ports are found.
/// @returns Selected COM port number (0-based index).
int select_COM_port_CLI(const bool autoselect) {
    SPDLOG_DEBUG("Selecting COM port with autoselect: {}", autoselect);
    int selected_cp{};
    std::vector<std::string> com_ports{};

    // Get available com ports
    try {
        com_ports = get_com_ports();
    } catch (std::runtime_error &e) {
        SPDLOG_ERROR(e.what());
        cli::err(e.what());
    }
    // End the program if there are none
    if (com_ports.empty()) {
        SPDLOG_ERROR(NO_PORTS);
        cli::err(std::string(NO_PORTS.data()) + "Make sure the NPET is connected and reset the program.");
        cli::confirm_exit();
        throw std::runtime_error("Error: No available COM ports found.");
    }
    // If there is only one COM port, select it automatically
    if (autoselect && com_ports.size() == 1) {
        SPDLOG_DEBUG("Autoselect enabled and only one COM port found: {}", com_ports[0]);
        // Extract only the COM port number
        selected_cp = com_ports[0].c_str()[com_ports[0].size() - 3] - '0';
    } else {
        // Print available comports
        cli::echo("--- Available COM ports ---", fg::gray, style::underline);
        for (const std::string &port: com_ports) {
            cli::echo(port);
        }
        // Choose the COM port
        const std::string com_port_str = cli::prompt("Choose COM port number");
        std::stringstream(com_port_str) >> selected_cp;
    }
    selected_cp--; // MS Windows correction
    SPDLOG_INFO("Selected CP num: {}", selected_cp);
    return selected_cp;
} // end of select_COM_port function


///
/// Open serial communication with NPET
/// Initial detection of the COM port is attempted first, followed by user prompt if detection fails.
void NPET_comm_CLI::open_communication_CLI() {
    constexpr int MAX_ATTEMPTS = 5;
    bool autoselect{true};

    SPDLOG_INFO("Opening NPET communication with CLI, max attempts: {}", MAX_ATTEMPTS);
    for (int i = 0; i < MAX_ATTEMPTS; i++) {
        // Limit the number of attempts to 5
        // After 2 failed attempts, disable autoselect
        if (i == 2) autoselect = false;
        SPDLOG_DEBUG("Attempt {} to open NPET communication, autoselect: {}", i + 1, autoselect);
        const int com_port = select_COM_port_CLI(autoselect);
        // +1 needed for MS Windows correction
        cli::show_int("Opening the NPET communication on COM", com_port + 1);
        if (com_port < 1) {
            SPDLOG_ERROR(INVALID_COM_PORT);
            cli::err(INVALID_COM_PORT.data());
            continue;
        }
        try {
            open_communication(com_port);
        } catch (std::exception &e) {
            SPDLOG_ERROR(FAILED_OPEN_COM_PORT, e.what());
            cli::err(std::format(FAILED_OPEN_COM_PORT, e.what()));
            continue;
        } // end of try-catch block
        if (!safe_exec([&] { return is_responsive(); }, "is_responsive")) {
            cli::echo("COM port opened successfully", fg::yellow);
            SPDLOG_ERROR(NPET_NOT_RESPONDING);
            cli::err(NPET_NOT_RESPONDING.data());
            port.close();
            continue;
        }
        SPDLOG_INFO("NPET communication opened successfully");
        return;
    } // end of for loop
    SPDLOG_ERROR(FAILED_OPEN_COM_PORT_MAX_ATTEMPT, MAX_ATTEMPTS);
    cli::err(std::format(FAILED_OPEN_COM_PORT_MAX_ATTEMPT, MAX_ATTEMPTS));
} // end of open_NPET_communication function


///
/// Checks if the NPET device is connected to the specified COM port and responsive.
/// Several attempts are made with a delay between each attempt.
/// Prints the status to the CLI.
/// @return True if NPET is connected and responsive, otherwise false.
bool NPET_comm_CLI::is_responsive_CLI() {
    constexpr int MAX_ATTEMPTS = 5;
    constexpr int RETRY_DELAY_MS = 1000;

    SPDLOG_DEBUG("Checking NPET responsiveness; max attempts: {}; retry delay: {} ms", MAX_ATTEMPTS, RETRY_DELAY_MS);
    for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
        SPDLOG_DEBUG("Responsiveness check attempt: {}", attempt + 1);
        if (safe_exec([&] { return is_responsive(); }, "is_responsive")) {
            SPDLOG_DEBUG(NPET_OK_RESPONDING);
            std::cout << std::endl; // Empty line
            cli::echo(NPET_OK_RESPONDING.data(), fg::green);
            return true;
        }
        SPDLOG_ERROR(NPET_NOT_RESPONDING);
        cli::err(NPET_NOT_RESPONDING.data());
        SPDLOG_DEBUG("Cancelling pending comms and purging all buffers before retrying ...");
        port.cancel();
        PurgeComm(port.native_handle(), PURGE_RXCLEAR | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_TXABORT);
        Sleep(RETRY_DELAY_MS);
    } // end of for loop
    SPDLOG_ERROR("NPET is not responsive after {} attempts", MAX_ATTEMPTS);
    return false;
} // end of is_NPET_connected_CLI function


///
/// Ask the user to select what version of FW the connected NPET device is running.
/// Firmware version is saved into the fw_version attribute.
void NPET_comm_CLI::set_FW_ver_CLI() {
    const std::vector<std::string> fw_options = {
        "Original",
        "Revision for NPET with AD component"
    };
    SPDLOG_DEBUG("{}: {}", FW_CURRENT, fw_version);
    cli::show_int(FW_CURRENT.data(), fw_version);
    SPDLOG_DEBUG("Selecting new version from: {}", fw_options);
    const int user_choice = cli::menu("What firmware is your NPET using?", fw_options, false);
    if (user_choice == 1) cli::echo("Selected the original NPET firmware.");
    else if (user_choice == 2) cli::echo("Selected the revised NPET firmware.");
    else {
        SPDLOG_ERROR(FW_INVALID);
        cli::err(FW_INVALID.data());
        return;
    }
    SPDLOG_DEBUG("Setting NPET firmware version to: {}", fw_options[user_choice - 1].data());
    safe_exec([&] { set_FW_ver(user_choice); }, "set_FW_ver");
} // end of select_NPET_FW_ver function


///
/// CLI wrapper for detect_FW_ver function.
void NPET_comm_CLI::detect_FW_ver_CLI() {
    SPDLOG_DEBUG("Detecting NPET firmware version ...");
    safe_exec([&] { detect_FW_ver(); }, "detect_FW_ver");
    if (fw_version == 1) cli::echo("Detected the original NPET firmware.");
    else if (fw_version == 2) cli::echo("Detected the FW revision for NPET with analogue devices component.");
    else if (fw_version == 3) cli::echo("Detected offline NPET", fg::yellow);
    else {
        SPDLOG_ERROR(FW_UNKNOWN);
        throw std::invalid_argument(FW_UNKNOWN.data());
    }
} // end of detect_FW_ver_CLI function


///
/// Data validation allows either a positive integer number or (optionally) -1 for infinity.
/// @param num_to_validate Number to check as string
/// @param allow_negative_one Whether to allow -1 as a valid input
/// @return The validated number in int format, or -2 if the input is invalid
int num_validation(const std::string &num_to_validate, const bool allow_negative_one = true) {
    int number_int;
    SPDLOG_DEBUG("Validating number: {}; allow infinity sentinel: {}", num_to_validate, allow_negative_one);

    try {
        // Attempt to convert the string to an integer
        number_int = std::stoi(num_to_validate);
        if (const int min_valid_value = allow_negative_one ? -1 : 0; number_int < min_valid_value) {
            throw std::invalid_argument("Number is out of allowed range");
        }
    } catch (const std::invalid_argument &) {
        SPDLOG_ERROR(INVALID_NUM);
        cli::err(INVALID_NUM.data());
        return INVALID_NUM_SENTINEL;
    }
    SPDLOG_DEBUG("Number {} is valid", number_int);
    return number_int;
} // end of is_valid_num_of_measurements_CLI function


///
/// CLI wrapper for the generate_pulses function.
/// Asks the user for the number of pulses to generate and the frequency.
void NPET_comm_CLI::generate_pulses_CLI() {
    SPDLOG_DEBUG("Generating pulses ...");
    // Set the number of pulses
    const std::string num_of_pulses_str = cli::prompt("Enter number of pulses (-1 for inf; 0 to stop generation)", "0");
    const int num_of_pulses = num_validation(num_of_pulses_str);
    if (num_of_pulses == INVALID_NUM_SENTINEL) return; // Invalid input, return to the main menu
    SPDLOG_DEBUG("User specified number for pulses: {}", num_of_pulses);
    // Set pulse generation frequency
    if (num_of_pulses != 0) {
        SPDLOG_DEBUG("Prompting user for pulse generation frequency ...");
        const std::string frequency_str = cli::prompt("Set pulse generation frequency [Hz] (0 to cancel)", "100");
        const int frequency_int = num_validation(frequency_str, false);
        if (frequency_int == INVALID_NUM_SENTINEL || frequency_int == 0) return; // Invalid input, return to the main menu
        SPDLOG_DEBUG("User specified pulse generation frequency: {} Hz", frequency_int);
        if (!safe_exec([&] { return set_frequency(frequency_int); }, "set_frequency")) {
            SPDLOG_ERROR(FREQ_ERR);
            cli::err(FREQ_ERR.data());
            return;
        }
        SPDLOG_DEBUG("{}: {}", FREQ_SET, frequency_int);
        cli::show_int(FREQ_SET.data(), frequency_int);
    }
    if (num_of_pulses == -1) cli::echo("Generating infinity pulses...");
    else if (num_of_pulses == 0) cli::echo("Stopping pulse generation");
    else cli::show_int("Generating pulses", num_of_pulses);
    if (safe_exec([&] { return generate_pulses(num_of_pulses); }, "generate_pulses")) {
        SPDLOG_INFO(PULSE_GEN_OK);
        cli::echo(PULSE_GEN_OK.data(), fg::green);
    } else {
        SPDLOG_ERROR(PULSE_GEN_ERR);
        cli::echo(PULSE_GEN_ERR.data(), fg::red);
    }
} // end of generate_pulses_handler function


///
/// ClI wrapper for the set_baud_rate function.
void NPET_comm_CLI::set_baud_rate_CLI() {
    SPDLOG_DEBUG("Setting baud rate ...");
    int new_baud_rate{};
    const std::vector<std::string> baud_rate_options = {
        "115200",
        "230400",
        "576000",
    };
    SPDLOG_DEBUG("Possible new baud rates: {}", baud_rate_options);
    // Get current baud rate
    boost::asio::serial_port_base::baud_rate current_baud{};
    port.get_option(current_baud);
    SPDLOG_DEBUG("{}: {}", BAUD_RATE_CURRENT, current_baud.value());
    cli::show_int(BAUD_RATE_CURRENT.data(), static_cast<int>(current_baud.value()));
    switch (cli::menu("New baud rate", baud_rate_options, false)) {
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
        cli::echo("YOU ARE ABOUT TO CHANGE THE COMMUNICATION BAUD RATE.", fg::yellow, style::bold);
        cli::echo("DO NOT DISCONNECT THE DEVICE OR CLOSE THE PROGRAM!", fg::yellow, style::bold);
        cli::echo("IF THIS PROCESS FAILS, RESTART THE DEVICE AND LAUNCH THIS PROGRAM ANEW.", fg::yellow, style::bold);
        Sleep(1000); // Wait for 1 second to let the user read the warning
        if (set_baud_rate(new_baud_rate)) {
            SPDLOG_DEBUG("{}: {}", BAUD_RATE_OK, new_baud_rate);
            cli::show_int(BAUD_RATE_OK.data(), new_baud_rate);
        }
        else {
            SPDLOG_ERROR(BAUD_RATE_ERR);
            cli::err(BAUD_RATE_ERR.data());
        }
    } catch (std::runtime_error &e) {
        SPDLOG_ERROR("{}: {}", BAUD_RATE_ERR, e.what());
        cli::err(BAUD_RATE_ERR.data() + std::string(e.what()));
        SPDLOG_WARN("Attempting to reopen the communication");
        cli::echo("Attempting to reopen the communication. If this keeps failing, reset the NPET!", fg::yellow);
        open_communication_CLI();
    } // end of try-catch block
} // end of set_baud_rate_handler function


///
/// CLI wrapper for the read_measurements function.
/// Asks the user for the number of measurements, channel to read from, display, and save options
void NPET_comm_CLI::read_batch_measurements_CLI() {
    SPDLOG_DEBUG("Reading batch measurements ...");
    // Prompt user for the number of measurements
    const std::string meas_num_str = cli::prompt("Number of measurements (-1 for inf; 0 to cancel)", "0");
    int num_of_meas = num_validation(meas_num_str);
    if (num_of_meas == 0 || num_of_meas == INVALID_NUM_SENTINEL) return;
    if (num_of_meas == -1) num_of_meas = INFINITE_OP; // Magic number for infinite measurements
    SPDLOG_DEBUG("User specified number of measurements to read: {}", num_of_meas);
    // Prompt user for channel number
    const std::string channel_str = cli::prompt("Select channel to read from (1 or 2; 0 to cancel)", "1");
    int channel{};
    try {
        channel = std::stoi(channel_str);
        if (channel == 0) return;
        if (channel < 0 || channel >= 3) throw std::invalid_argument(CHANNEL_INVALID.data());
    } catch (const std::invalid_argument &) {
        SPDLOG_ERROR(CHANNEL_INVALID);
        cli::err(CHANNEL_INVALID.data());
        return;
    } // end of try-catch block
    SPDLOG_DEBUG("User specified channel number: {}", channel);
    // Prompt user for the display and save options
    const std::string monitor_str = cli::prompt("Measurement monitoring (0 - None, 1 - Basic, 2 - Advanced, 3 - Sync)", "1");
    int monitor{};
    try {
        monitor = std::stoi(monitor_str);
    } catch (const std::invalid_argument &) {
        SPDLOG_ERROR(MONITOR_FN_INVALID);
        cli::err(MONITOR_FN_INVALID.data());
        return;
    } // end of try-catch block
    SPDLOG_DEBUG("User specified measurement monitoring: {}", monitor);
    std::function<void(measurement_reader&, const meas_context&, const measurement&)> monitor_fn;
    switch (monitor) {
        case 1: monitor_fn = reader_cli_basic; break;
        case 2: monitor_fn = reader_cli_advanced; break;
        case 3: monitor_fn = reader_cli_sync; break;
        default: monitor_fn = nullptr; break;
    }
    SPDLOG_DEBUG("Monitoring function successfully assigned");
    const bool save_flag = cli::confirm("Save the measurements?", true);
    SPDLOG_DEBUG("User specified save measurements flag: {}", save_flag);
    // Prepare measurement context object
    const meas_context meas_settings{
        .num_of_meas = num_of_meas,
        .monitor_fn = monitor_fn,
        .save = save_flag,
        .channel = channel
    };
    SPDLOG_DEBUG("Measurement context object: {}", meas_settings.to_string());
    safe_exec([&] { read_batch_measurements(meas_settings); }, "read_batch_measurements");
    SPDLOG_INFO("Finished reading measurements");
} // end of read_measurements_handler function


///
/// Set the time correction constant on the NPET device.
/// User can choose between raw format input, time format input, or clearing the constant.
/// Either way, the time correction constant saved in the NPET and in the program is always the same.
/// In the end, the constant is exported to the NPET and sample measurements are read to show the effect.
void NPET_comm_CLI::set_time_constant_CLI() {
    SPDLOG_DEBUG("Setting time correction constant ...");
    measurement new_const{-1};
    const std::vector<std::string> definition_options = {
        "Raw format: int_sec frac_sec",
        "Time format: hh:mm:ss",
        "Clear time constant",
        "Cancel",
    };
    const std::vector<std::string> int_options = {
        "User defined",
        "System time",
        "System time with NTP sync (Admin required)",
        "Cancel"
    };

    SPDLOG_DEBUG("Possible new time correction constant definitions: {}", definition_options);
    switch (cli::menu("Time correction constant definition", definition_options, false)) {
        case 1:
            SPDLOG_DEBUG("User selected raw format definition for time correction constant");
            try {
                new_const = raw_time_constant();
            } catch (std::exception &e) {
                SPDLOG_ERROR(TIME_CONST_FAILED_TO_SET, e.what());
                cli::err(std::format(TIME_CONST_FAILED_TO_SET.data(), e.what()));
                return;
            }
            break;
        case 2: {
            SPDLOG_DEBUG("User selected time format definition for time correction constant");
            const int aver_num = std::stoi(cli::prompt("Number of averaging measurements", "16"));
            SPDLOG_DEBUG("User specified number of measurements for averaging: {}", aver_num);
            const int pps_channel = std::stoi(cli::prompt("What channel is the PPS connected to?", "2"));
            SPDLOG_DEBUG("User specified PPS channel: {}", pps_channel);
            cli::echo("Beginning the measurement ...");
            new_const.fracp = measure_average_fraction(aver_num, pps_channel);
            if (new_const.fracp == -0.0) {
                // Failed to take any measurements
                SPDLOG_ERROR(TIME_CONST_FRAC_MEAS_ERR);
                cli::err(TIME_CONST_FAILED_TO_SET.data());
                SPDLOG_DEBUG("Breaking measurement stream ...");
                (void) is_responsive(true); // Break measurement stream
                return;
            }
            SPDLOG_DEBUG("Measured average fractional part of the time correction constant: {}", float128_to_string(new_const.fracp));
            SPDLOG_DEBUG("Prompting user for integer part definition logic ...");
            SPDLOG_DEBUG("Possible integer part definition logic options: {}", int_options);
            const int int_choice = cli::menu("Integer part setting logic", int_options, false);
            SPDLOG_DEBUG("User selected integer part definition logic: {}", int_choice);
            if (int_choice == 4) return;
            new_const.intp = calc_integer(int_choice, pps_channel);
            SPDLOG_DEBUG("Calculated integer part of the time correction constant: {}", new_const.intp);
            break;
        }
        case 3:
            SPDLOG_DEBUG("User selected to clear the time correction constant");
            if (!safe_exec([&] { return clear_time_constant(); }, "clear_time_constant")) {
                SPDLOG_ERROR(TIME_CONST_FAILED_TO_CLEAR);
                cli::err(TIME_CONST_FAILED_TO_CLEAR.data());
            } else {
                SPDLOG_INFO(TIME_CONST_CLEAR_OK);
                cli::echo(TIME_CONST_CLEAR_OK.data(), fg::green);
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
        cli::err(TIME_CONST_INVALID.data());
        return;
    }
    if (!safe_exec([&] { return export_time_constant(new_const); }, "export_time_constant")) {
        SPDLOG_ERROR(TIME_CONST_FAILED_TO_EXPORT);
        cli::err(TIME_CONST_FAILED_TO_EXPORT.data());
        return;
    }
    SPDLOG_INFO("{}: {}", TIME_CONST_SET, new_const.to_string());
    cli::show_str(TIME_CONST_SET.data(), new_const.to_string());
    // Read sample measurements to see the results
    SPDLOG_DEBUG("Reading sample measurements to show the effect of the new time correction constant ...");
    safe_exec([&] { read_batch_measurements(meas_context{.num_of_meas = 10, .monitor_fn = reader_cli_sync, .channel = 2}); },
              "read_batch_measurements");
} // end of set_time_constant_handler function


///
/// Ask the user to provide the time correction constant in raw format [int, frac].
/// It is also possible to adjust the existing value by a specified number of seconds.
/// @return The new time correction constant in measurement format.
measurement NPET_comm_CLI::raw_time_constant() {
    SPDLOG_DEBUG("Defining time correction constant in raw format ...");
    measurement new_const{-1}; // Measurement num -1 marks the measurement as a time correction constant
    std::string input_string{};

    SPDLOG_DEBUG("Importing existing time correction constant from NPET ...");
    const measurement old_const = safe_exec([&] { return import_time_constant(); }, "import_time_constant");
    SPDLOG_INFO("{}: {}", TIME_CONST_CURRENT, old_const.to_string());
    cli::show_str(TIME_CONST_CURRENT.data(), old_const.to_string());
    cli::echo("Insert new time correction constant (or adjust the existing value)");
    // Either enter the new value of seconds or adjust the new one by +- value
    SPDLOG_DEBUG("Prompting user for new time correction constant or adjustment ...");
    input_string = cli::prompt("Seconds (0 to cancel, +-value to adjust)", "0");
    SPDLOG_DEBUG("User input for time correction constant: {}", input_string);
    if (input_string == "0") return measurement{-2}; // Return an invalid measurement to indicate cancellation
    // Handle constant adjustment
    if (const char sign = input_string[0]; sign == '+' || sign == '-') {
        SPDLOG_DEBUG("User input indicates an adjustment of the existing time correction constant");
        // Extract the number of seconds to adjust
        const int seconds_adjustment = std::stoi(input_string.substr(1));
        // Adjust the constant_secs value
        SPDLOG_DEBUG("{}: {}{}", TIME_CONST_ADJUST, sign, seconds_adjustment);
        cli::show_str(TIME_CONST_ADJUST.data(), input_string);
        new_const = old_const; // Copy the old constant
        if (sign == '+') new_const.intp += seconds_adjustment;
        else new_const.intp -= seconds_adjustment;
    } else {
        SPDLOG_DEBUG("User input indicates new time correction constant definition");
        // Handle new constant definition
        new_const.intp = std::stoi(input_string);
        SPDLOG_DEBUG("New constant int part: {}", new_const.intp);
        input_string = cli::prompt("Fraction of a second (0.xxx format)", "0.0");
        const __float128 frac_part = strtoflt128(input_string.c_str(), nullptr); // Convert input to float
        if (!(0 < frac_part && frac_part < 1)) {
            SPDLOG_ERROR("{}: {}", TIME_CONST_SET_FRAC_PART, float128_to_string(frac_part));
            cli::err(TIME_CONST_SET_FRAC_PART.data());
            return measurement{-2};
        }
        new_const.fracp = frac_part;
        SPDLOG_DEBUG("New constant fractional part: {}", float128_to_string(new_const.fracp));
    } // end of if-else block
    SPDLOG_INFO("New time correction constant: {}", new_const.to_string());
    return new_const;
} // end of raw_time_constant_CLI function


/// Measure the average fractional part of the time correction constant.
/// User is prompted for the number of averages and the PPS channel to use.
/// @param aver_num Number of measurements to average
/// @param channel_num Channel number to read the measurements from (1 or 2)
/// @return The fractional part of the time correction constant
__float128 NPET_comm_CLI::measure_average_fraction(const int aver_num, const int channel_num) {
    SPDLOG_DEBUG(TIME_CONST_FRAC_MEAS);
    __float128 sum{};

    cli::echo(TIME_CONST_FRAC_MEAS.data());
    // For higher precision, take n measurements and compute the average fractional number of seconds
    SPDLOG_DEBUG("Beginning fractional part measurement of {} averages from channel {}", aver_num, channel_num);
    auto bar = ProgressBar(aver_num);
    for (int i = aver_num; i > 0; i--) {
        if (_kbhit()) {
            SPDLOG_ERROR(TIME_CONST_FRAC_MEAS_INTERRUPT);
            cli::err(TIME_CONST_FRAC_MEAS_INTERRUPT.data());
            return false;
        }
        const measurement meas = safe_exec([&] { return read_single_measurement(channel_num); },
                                           "read_single_measurement");
        sum += meas.fracp;
        bar.update(aver_num - i + 1);
    } // end of for loop
    SPDLOG_DEBUG("Finished taking measurements for fractional part. Sum of fractional parts: {}", float128_to_string(sum));
    cli::echo(""); // New line after progress bar
    // Return the negative average fractional part
    // which will be used to compensate for the offset
    const __float128 neg_avg_frac = -sum / aver_num;
    SPDLOG_INFO("Computed time correction constant fractional part: {}", float128_to_string(neg_avg_frac));
    return neg_avg_frac;
} // end of measure_average_fraction_CLI function


///
/// Define the integer part of the time correction constant.
/// @param int_logic Either ask the user to define the integer part of the time constant or grab system time
/// @param channel_num Channel number to read the measurements from (1 or 2). Only used if int_logic is 1 or 2.
/// @return The integer part of the time correction constant
int NPET_comm_CLI::calc_integer(const int int_logic, const int channel_num) {
    SPDLOG_DEBUG("Calculating integer part of the time correction constant with logic id: {}", int_logic);
    int user_choice{};
    int clock_seconds{};

    switch (int_logic) {
        case 1:
            SPDLOG_DEBUG("Logic: Define manually, user will be prompted to enter the target time ...");
            // User defined integer part
            cli::echo("Enter time of the next 1 Hz measurement in hh:mm:ss format");
            cli::echo(
                "You will be asked to confirm the values after inputting ss, the calibration will begin once you've confirmed.",
                fg::yellow);
            user_choice = std::stoi(cli::prompt("Hours", "0"));
            clock_seconds = 3600 * user_choice;
            user_choice = std::stoi(cli::prompt("Minutes", "0"));
            clock_seconds += 60 * user_choice;
            user_choice = std::stoi(cli::prompt("Seconds", "0"));
            clock_seconds += user_choice;
            SPDLOG_DEBUG("User specified target time in seconds since midnight: {}", clock_seconds);
            cli::show_int("Defined clock seconds", clock_seconds);
            SPDLOG_DEBUG("Awaiting final user confirmation for the calibration time");
            if (const bool confirm = cli::confirm(
                "Confirm by pressing `Enter` when the designated time is about to happen",
                true
            ); confirm == false) { // Cancel the calibration
                SPDLOG_ERROR("User cancelled the time correction constant integer part calibration");
                return 0;
                }
            SPDLOG_DEBUG("Final confirmation received, time correction constant integer part will be set to the defined clock seconds");
            break;
        case 3:
            SPDLOG_DEBUG("Logic: Synchronize system time with NTP server ...");
            // Query an NTP server for the current time
            if (!ensure_accurate_system_time()) cli::err("Failed to synchronize system time with NTP server");
        // Intentional fallthrough to case 2
        case 2: {
            SPDLOG_DEBUG("Logic: Use system time ...");
            // Get the current system time
            cli::echo("Calculating time correction integer constant from system time");
            const std::time_t now = std::time(nullptr);
            const std::tm *local_time = std::localtime(&now);
            std::ostringstream oss;
            oss << std::put_time(local_time, "%H:%M:%S");
            SPDLOG_DEBUG("{}: {}", SYSTEM_TIME_CURRENT, oss.str());
            cli::show_str(SYSTEM_TIME_CURRENT.data(), oss.str());
            // Calculate seconds since midnight
            clock_seconds = local_time->tm_hour * 3600 + local_time->tm_min * 60 + local_time->tm_sec;
            SPDLOG_DEBUG("Calculated seconds since midnight: {}", clock_seconds);
            break;
        }
        default:
            SPDLOG_ERROR(INVALID_NUM);
            cli::err(INVALID_NUM.data());
            return 0;
    } // end of switch
    // Get the current NPET time
    SPDLOG_DEBUG("Reading current measurement from channel {} to get the NPET time ...", channel_num);
    const measurement current_measurement = safe_exec([&] { return read_single_measurement(channel_num); },
                                                      "read_single_measurement");
    SPDLOG_DEBUG("Current measurement read: {}", current_measurement.to_string());
    // Integer part of the time correction constant is the difference between the clock time and the measured time
    const int integer_part = clock_seconds - current_measurement.intp;
    SPDLOG_DEBUG("Calculated integer part of the time correction constant: {}", integer_part);
    return integer_part;
} // end of calc_integer_CLI function


///
/// Reset NPET into default settings.
void NPET_comm_CLI::reset_CLI() {
    SPDLOG_INFO(RESET_INITIATED);
    cli::echo(RESET_INITIATED.data(), fg::yellow);
    if (!safe_exec([&] { return clear_time_constant(); }, "clear_time_constant")) {
        SPDLOG_ERROR(TIME_CONST_FAILED_TO_CLEAR);
        cli::err(TIME_CONST_FAILED_TO_CLEAR.data());
    }
    if (!safe_exec([&] { return generate_pulses(0); }, "generate_pulses")) {
        SPDLOG_ERROR(PULSE_GEN_STOP_FAIL);
        cli::err(PULSE_GEN_STOP_FAIL.data());
    }
    if (!safe_exec([&] { return set_frequency(); }, "set_frequency")) {
        SPDLOG_ERROR(FREQ_RESET_FAIL);
        cli::err(FREQ_RESET_FAIL.data());
    }
    if (!safe_exec([&] { return set_measured_data_format(1); }, "set_measured_data_format")) {
        SPDLOG_ERROR(DATA_FORMAT_RESET_FAIL);
        cli::err(DATA_FORMAT_RESET_FAIL.data());
    }
    if (!safe_exec([&] { return set_baud_rate(); }, "set_baud_rate")) {
        SPDLOG_ERROR(BAUD_RATE_RESET_FAIL);
        cli::err(BAUD_RATE_RESET_FAIL.data());
    }
    SPDLOG_INFO(RESET_COMPLETE);
    cli::echo(RESET_COMPLETE.data(), fg::green);
} // end of reset_NPET function
