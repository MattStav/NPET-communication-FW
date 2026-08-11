#include "NPET_comm_CLI.h"

#include <conio.h>
#include <spdlog/fmt/ranges.h>  // enables formatting of vectors, arrays, etc.

#include "meas_reader_CLI.h"
#include "helper_func.h"

constexpr std::string_view NPET_OK_RESPONDING = "NPET communication is OK";
constexpr std::string_view NPET_NOT_RESPONDING = "NPET not responding!";
constexpr std::string_view FW_UNKNOWN = "Unknown firmware version detected";
constexpr std::string_view FREQ_ERR = "Couldn't set the frequency";
constexpr std::string_view FREQ_SET = "Pulse generation frequency set to [Hz]";
constexpr std::string_view PULSE_GEN_OK = "Pulse generation successful";
constexpr std::string_view PULSE_GEN_ERR = "Pulse generation failed";
constexpr std::string_view TIME_CONST_SET = "New time correction constant set to";
constexpr std::string_view RESET_INITIATED = "Resetting NPET to default settings";
constexpr std::string_view RESET_COMPLETE = "NPET reset sequence finished";


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
        if (const int COM_PORT = selectComPortCLI(autoselect); !openCommSafe(*this, COM_PORT, NPET_NOT_RESPONDING)) {
            continue;
        }
        SPDLOG_INFO("NPET communication opened successfully");
        return;
    } // end of for loop
    SPDLOG_ERROR(FAILED_OPEN_COM_PORT_MAX_ATTEMPT, MAX_ATTEMPTS);
    Cli::err(std::format(FAILED_OPEN_COM_PORT_MAX_ATTEMPT, MAX_ATTEMPTS));
    Cli::confirmExit();
    throw std::runtime_error(std::format(FAILED_OPEN_COM_PORT_MAX_ATTEMPT, MAX_ATTEMPTS));
} // end of open_NPET_communication function


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
        ser.purgePort();
        Sleep(RETRY_DELAY_MS);
    } // end of for loop
    SPDLOG_ERROR("NPET is not responsive after {} attempts", MAX_ATTEMPTS);
    return false;
} // end of is_NPET_connected_CLI function


void NPETCommCLI::setFwVerCLI() {
    const FWVersion NEW_FW = promptFWVersion(getFWVer());
    SPDLOG_DEBUG("Setting NPET firmware version to: {}", NEW_FW.getDescription());
    safeExec([&] { setFWVer(NEW_FW); }, "set_FW_ver");
} // end of select_NPET_FW_ver function


void NPETCommCLI::detectFwVerCLI() {
    SPDLOG_DEBUG("Detecting NPET firmware version ...");
    safeExec([&] { detectFWVer(); }, "detect_FW_ver");
    try {
        const std::string DESCRIPTION = "Detected firmware: " + std::string(getFWVer().getDescription());
        if (getFWVer().getValue() == FWVersion::VIRTUAL) {
            Cli::echo(DESCRIPTION, fg::yellow);
        } else {
            Cli::echo(DESCRIPTION);
        }
    } catch (const std::invalid_argument &) {
        SPDLOG_ERROR(FW_UNKNOWN);
        throw std::invalid_argument(std::string(FW_UNKNOWN));
    }
} // end of detect_FW_ver_CLI function


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


void NPETCommCLI::setBaudRateCLI() {
    SPDLOG_DEBUG("Setting baud rate ...");
    const int CURRENT_BAUD = ser.getBaudRate();
    const int NEW_BAUD_RATE = promptBaudRate(CURRENT_BAUD);
    if (NEW_BAUD_RATE == CURRENT_BAUD) {
        SPDLOG_DEBUG(BAUD_RATE_ALREADY_SET);
        Cli::echo(std::string(BAUD_RATE_ALREADY_SET));
        return;
    }
    setBaudRateSafe(*this, NEW_BAUD_RATE);
} // end of set_baud_rate_handler function


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
    const auto CHANNEL = promptChannel(1);
    if (!CHANNEL.has_value()) {
        return;
    }
    SPDLOG_DEBUG("User specified channel number: {}", static_cast<int>(CHANNEL.value()));
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
    std::optional<std::filesystem::path> save_path;
    if (SAVE_FLAG) {
        save_path = std::filesystem::path(outputFilePath(CHANNEL.value(), USER_FILES));
    } else {
        save_path = std::nullopt;
    }
    SPDLOG_DEBUG("Measurements will be saved to: {}", save_path ? save_path->string() : "none");
    // Prepare measurement context object
    const MeasContext MEAS_SETTINGS{
        .num_of_meas = num_of_meas,
        .monitor_fn = monitor_fn,
        .save_path = save_path,
        .channel = CHANNEL.value(),
    };
    SPDLOG_DEBUG("Measurement context object: {}", MEAS_SETTINGS.toString());
    safeExec([&] { readBatchMeasurements(MEAS_SETTINGS); }, "read_batch_measurements");
    SPDLOG_INFO("Finished reading measurements");
} // end of read_measurements_handler function


void NPETCommCLI::setTimeConstantCLI() {
    SPDLOG_DEBUG("Setting time correction constant ...");
    Measurement new_const{.meas_num = -1};
    const std::vector<std::string> DEFINITION_OPTIONS = {
        "Raw format: int_sec frac_sec",
        "Time format: hh:mm:ss",
        "Clear time constant",
        "Cancel",
    };

    SPDLOG_DEBUG("Possible new time correction constant definitions: {}", DEFINITION_OPTIONS);
    switch (Cli::menu("Time correction constant definition", DEFINITION_OPTIONS, false)) {
        case 1: {
            SPDLOG_DEBUG("User selected raw format definition for time correction constant");
            const Measurement OLD_CONST = safeExec([&] { return importTimeConstant(); }, "import_time_constant");
            new_const = promptRawTimeConstant(OLD_CONST);
            break;
        }
        case 2: {
            SPDLOG_DEBUG("User selected time format definition for time correction constant");
            const std::string MEAS_NUM_STR = Cli::prompt("Number of averaging measurements", "16");
            int AVER_NUM = numValidation(MEAS_NUM_STR, false);
            if (AVER_NUM == INVALID_NUM_SENTINEL) {
                return;
            }
            SPDLOG_DEBUG("User specified number of measurements for averaging: {}", AVER_NUM);
            const auto PPS_CHANNEL = promptChannel(2, "the PPS channel");
            if (!PPS_CHANNEL.has_value()) {
                return;
            }
            SPDLOG_DEBUG("User specified PPS channel: {}", static_cast<int>(PPS_CHANNEL.value()));
            SPDLOG_DEBUG(TIME_CONST_FRAC_MEAS);
            Cli::echo(std::string(TIME_CONST_FRAC_MEAS));
            auto bar = ProgressBar({.total = AVER_NUM});
            const std::optional<__float128> AVE_FRAC = getAverageFraction(AVER_NUM, PPS_CHANNEL.value(), &bar);
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
            // Export the fractional part to the NPET
            safeExec([&] { return exportTimeConstant(new_const); }, "export_time_constant");
            SPDLOG_DEBUG("Prompting user for integer part definition logic ...");
            SPDLOG_DEBUG("Possible integer part definition logic options: {}", INT_OPTIONS);
            const int INT_CHOICE = Cli::menu("Integer part setting logic", INT_OPTIONS, false);
            SPDLOG_DEBUG("User selected integer part definition logic: {}", INT_CHOICE);
            if (INT_CHOICE == 4) {
                return;
            }
            SPDLOG_DEBUG("Calculating integer part of the time correction constant with logic id: {}", INT_CHOICE);
            const int CLOCK_TIME = promptTimeConstSeconds(static_cast<ConstIntSelectionLogic>(INT_CHOICE));
            // Get the current NPET time
            SPDLOG_DEBUG("Reading current measurement from channel {} to get the NPET time ...",
                         static_cast<int>(PPS_CHANNEL.value()));
            const Measurement CURRENT_MEASUREMENT = safeExec([&] { return readSingleMeasurement(PPS_CHANNEL.value()); },
                                                             "read_single_measurement");
            SPDLOG_DEBUG("Current measurement read: {}", CURRENT_MEASUREMENT.toString());
            // Integer part of the time correction constant is the difference between the clock time and the measured time
            new_const.intp = CLOCK_TIME - CURRENT_MEASUREMENT.intp;
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


void NPETCommCLI::resetCLI() {
    SPDLOG_INFO(RESET_INITIATED);
    Cli::echo(std::string(RESET_INITIATED), fg::yellow);
    resetNPETSafe(*this);
    SPDLOG_INFO(RESET_COMPLETE);
    Cli::echo(std::string(RESET_COMPLETE), fg::green);
} // end of reset_NPET function
