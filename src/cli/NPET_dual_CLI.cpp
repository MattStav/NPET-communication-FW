#include "NPET_dual_CLI.h"

#include <spdlog/fmt/ranges.h>  // enables formatting of vectors, arrays, etc.

#include "meas_reader_CLI.h"


constexpr std::string_view NPET_DUAL_OK_RESPONDING = "NPETs communication is OK";
constexpr std::string_view NPET_DUAL_NOT_RESPONDING = "Both NPETs not responding!";
constexpr std::string_view NPET_START_NOT_RESPONDING = "Start NPET not responding!";
constexpr std::string_view NPET_STOP_NOT_RESPONDING = "Stop NPET not responding!";
constexpr std::string_view NPET_DESIGNATION_SWITCH_INIT = "Switching NPET START/STOP designation ...";
constexpr std::string_view NPET_DESIGNATION_SWITCH_DONE = "NPET START/STOP designation switched successfully";
constexpr std::string_view NPET_SETTING_BAUD_RATE = "Now setting baud rate for the {} NPET";
constexpr std::string_view DUAL_RESET_INITIATED = "Resetting both NPETs to default settings";
constexpr std::string_view DUAL_RESET_COMPLETE = "NPETs reset sequence finished";
constexpr std::string_view START_TIME_CONST_SET = "New START time correction constant set to";
constexpr std::string_view STOP_TIME_CONST_SET = "New STOP time correction constant set to";

///
/// Take the NPET the user has selected as START/STOP,
/// set it to channel 2 and have user confirm that the expected NPET is truly in channel 2.
/// @param npet The NPETComm reference
/// @param designation The designation of the NPET (e.g., "START" or "STOP")
/// @return True if the NPET expected NPET is set to channel 2, False otherwise
static bool confirmNPETSelection(NPETComm &npet, const std::string &designation) {
    SPDLOG_DEBUG("Confirming that the {} NPET was correctly selected", designation);
    safeExec([&] { return npet.readSingleMeasurement(Channel::CH2); }, "confirm_npet_selection");
    const bool RET = Cli::confirm("Confirm that the " + designation + " NPET is currently set to channel 2?", true);
    safeExec([&] { return npet.readSingleMeasurement(Channel::CH1); }, "confirm_npet_selection");
    SPDLOG_DEBUG("Confirmed that the {} was selected {}", designation, RET ? "CORRECTLY" : "WRONGLY");
    return RET;
}


///
/// Run the COM Port selection, connection and validation in loop.
/// The loop ends when NPET is correctly selected or max attempts are reached,
/// in which case the function throws.
/// @param npet The NPET reference.
/// @param designation The NPET designation name (START/STOP)
/// @param ERROR_MSG Error message to print if NPET is not responsive
/// @param EXCLUDED_PORT COM Port to exclude from selection (default 0)
static int openCommLoop(NPETComm &npet, const std::string &designation, const std::string_view ERROR_MSG,
                        const int EXCLUDED_PORT = 0) {
    constexpr int MAX_ATTEMPTS = 5;
    bool autoselect{true};
    auto excluded_ports = std::vector{EXCLUDED_PORT};

    SPDLOG_INFO("Opening {} NPET communication with CLI, max attempts: {}", designation, MAX_ATTEMPTS);
    for (int i = 0; i < MAX_ATTEMPTS; i++) {
        // After 2 failed attempt, disable autoselect
        if (i == 2) {
            autoselect = false;
        }
        SPDLOG_DEBUG("Attempt {} to open {} NPET communication", i + 1, designation);
        Cli::echo("Select the COM port number for the " + designation + " NPET", fg::gray, style::bold);
        const int COM_PORT = selectComPortCLI(autoselect, excluded_ports);
        if (!openCommSafe(npet, COM_PORT, ERROR_MSG)) {
            continue;
        }
        npet.detectFWVer();
        if (!confirmNPETSelection(npet, designation)) {
            npet.ser.closeCommunication();
            excluded_ports.push_back(COM_PORT);
            continue;
        }
        SPDLOG_INFO("{} NPET communication opened successfully", designation);
        return COM_PORT;
    } // end of for loop
    SPDLOG_ERROR(FAILED_OPEN_COM_PORT_MAX_ATTEMPT, MAX_ATTEMPTS);
    Cli::err(std::format(FAILED_OPEN_COM_PORT_MAX_ATTEMPT, MAX_ATTEMPTS));
    Cli::confirmExit();
    throw std::runtime_error(std::format(FAILED_OPEN_COM_PORT_MAX_ATTEMPT, MAX_ATTEMPTS));
}


void NPETDualCLI::openCommunicationCLI() {
    SPDLOG_INFO("Opening both NPETs communication with CLI");
    const int START_PORT = openCommLoop(one_, "START", NPET_START_NOT_RESPONDING);
    openCommLoop(two_, "STOP", NPET_STOP_NOT_RESPONDING, START_PORT);
    SPDLOG_INFO("Both NPETs communication opened successfully");
} // end of open_NPET_communication function


bool NPETDualCLI::bothResponsiveCLI() {
    constexpr int MAX_ATTEMPTS = 5;
    constexpr int RETRY_DELAY_MS = 1000;
    bool start_res{false};
    bool stop_res{false};

    SPDLOG_DEBUG("Checking NPETs responsiveness; max attempts: {}; retry delay: {} ms", MAX_ATTEMPTS, RETRY_DELAY_MS);
    for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
        SPDLOG_DEBUG("Responsiveness check attempt: {}", attempt + 1);
        if (!start_res) {
            start_res = safeExec([&] { return start().isResponsive(); }, "is_start_responsive");
        }
        if (!stop_res) {
            stop_res = safeExec([&] { return stop().isResponsive(); }, "is_stop_responsive");
        }
        if (start_res && stop_res) {
            SPDLOG_DEBUG(NPET_DUAL_OK_RESPONDING);
            std::cout << '\n'; // Empty line
            Cli::echo(std::string(NPET_DUAL_OK_RESPONDING), fg::green);
            return true;
        }
        SPDLOG_DEBUG("Cancelling pending comms and purging all buffers before retrying ...");
        if (!start_res && !stop_res) {
            SPDLOG_ERROR(NPET_DUAL_NOT_RESPONDING);
            Cli::err(std::string(NPET_DUAL_NOT_RESPONDING));
            start().ser.purgePort();
            stop().ser.purgePort();
        } else if (!start_res) {
            SPDLOG_ERROR(NPET_START_NOT_RESPONDING);
            Cli::err(std::string(NPET_START_NOT_RESPONDING));
            start().ser.purgePort();
        } else {
            SPDLOG_ERROR(NPET_STOP_NOT_RESPONDING);
            Cli::err(std::string(NPET_STOP_NOT_RESPONDING));
            stop().ser.purgePort();
        }
        Sleep(RETRY_DELAY_MS);
    } // end of for loop
    SPDLOG_ERROR("NPETs are not responsive after {} attempts", MAX_ATTEMPTS);
    return false;
} // end of is_NPET_connected_CLI function


void NPETDualCLI::setFwVerCLI() {
    Cli::echo("Select the START NPET firmware version");
    const FWVersion NEW_FW = promptFWVersion(start().getFWVer());
    SPDLOG_DEBUG("Setting START NPET firmware version to: {}", NEW_FW.getDescription());
    safeExec([&] { start().setFWVer(NEW_FW); }, "set_FW_ver");
    Cli::echo("");
    Cli::echo("Select the STOP NPET firmware version");
    const FWVersion NEW_FW_TWO = promptFWVersion(stop().getFWVer());
    SPDLOG_DEBUG("Setting STOP NPET firmware version to: {}", NEW_FW_TWO.getDescription());
    safeExec([&] { stop().setFWVer(NEW_FW_TWO); }, "set_FW_ver");
}


void NPETDualCLI::readBatchMeasurementsCLI() {
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
    const auto START_CHANNEL = promptChannel(1, "START NPET channel");
    if (!START_CHANNEL.has_value()) {
        return;
    }
    SPDLOG_DEBUG("User specified channel number: {}", static_cast<int>(START_CHANNEL.value()));
    const auto STOP_CHANNEL = promptChannel(1, "STOP NPET channel");
    if (!STOP_CHANNEL.has_value()) {
        return;
    }
    SPDLOG_DEBUG("User specified channel number: {}", static_cast<int>(STOP_CHANNEL.value()));
    // Prompt user for the display and save options
    const std::string MONITOR_STR = Cli::prompt("Measurement monitoring (0 - None, 1 - Basic, 2 - Advanced, 3 - Diff)",
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
    std::function<void(DualMeasReader &, const DualMeasContext &, const Measurement &start_time_const,
                       const Measurement &stop_time_const)>
            monitor_fn;
    switch (monitor) {
        case 1: monitor_fn = dualReaderCliBasic;
            break;
        case 2: monitor_fn = dualReaderCliBasic;
            // TODO: Implement advanced monitor
            Cli::err("Not implemented yet, defaulting to basic monitoring");
            break;
        case 3: monitor_fn = dualReaderCliDiff;
            break;
        default: monitor_fn = nullptr;
            break;
    }
    SPDLOG_DEBUG("Monitoring function successfully assigned");
    const bool SAVE_FLAG = Cli::confirm("Save the measurements?", true);
    SPDLOG_DEBUG("User specified save measurements flag: {}", SAVE_FLAG);
    // Prepare measurement context object
    const DualMeasContext MEAS_SETTINGS{
        .num_of_meas = num_of_meas,
        .monitor_fn = monitor_fn,
        .save_dir = SAVE_FLAG ? std::optional{USER_FILES} : std::nullopt,
        .start_channel = START_CHANNEL.value(),
        .stop_channel = STOP_CHANNEL.value(),
    };
    SPDLOG_DEBUG("Measurement context object: {}", MEAS_SETTINGS.toString());
    safeExec([&] { readBatchMeasurements(MEAS_SETTINGS); }, "read_batch_measurements");
    SPDLOG_INFO("Finished reading measurements");
} // end of read_measurements_handler function


void NPETDualCLI::switchStartStopCLI() {
    SPDLOG_DEBUG(NPET_DESIGNATION_SWITCH_INIT);
    Cli::echo(std::string(NPET_DESIGNATION_SWITCH_INIT));
    switchStartStop();
    Cli::echo(std::string(NPET_DESIGNATION_SWITCH_DONE), fg::green);
    SPDLOG_INFO(NPET_DESIGNATION_SWITCH_DONE);
}


void NPETDualCLI::setBaudRateCLI() {
    SPDLOG_DEBUG("Setting baud rate on both NPETs ...");
    const int CURRENT_BAUD_ONE = one_.ser.getBaudRate();
    const int CURRENT_BAUD_TWO = one_.ser.getBaudRate();
    assert(CURRENT_BAUD_ONE == CURRENT_BAUD_TWO);
    const int NEW_BAUD_RATE = promptBaudRate(CURRENT_BAUD_ONE);
    SPDLOG_DEBUG("User specified new baud rate: {}", NEW_BAUD_RATE);
    if (NEW_BAUD_RATE == CURRENT_BAUD_ONE) {
        SPDLOG_DEBUG(BAUD_RATE_ALREADY_SET);
        Cli::echo(std::string(BAUD_RATE_ALREADY_SET));
        return;
    }
    SPDLOG_DEBUG(NPET_SETTING_BAUD_RATE, "START");
    Cli::echo(std::format(NPET_SETTING_BAUD_RATE, "START NPET"));
    setBaudRateSafe(start(), NEW_BAUD_RATE, "START");
    SPDLOG_DEBUG(NPET_SETTING_BAUD_RATE, "STOP");
    Cli::echo(std::format(NPET_SETTING_BAUD_RATE, "STOP NPET"));
    setBaudRateSafe(stop(), NEW_BAUD_RATE, "STOP");
    SPDLOG_INFO("Baud rate successfully set to {}", NEW_BAUD_RATE);
}


void NPETDualCLI::syncNPETsCLI() {
    SPDLOG_DEBUG("Setting time correction constant ...");
    Measurement start_const{.meas_num = -1};
    Measurement stop_const{.meas_num = -1};
    const std::vector<std::string> DEFINITION_OPTIONS = {
        "Adjust single raw constant",
        "Mutual synchronization",
        "Clear time constants",
        "Cancel",
    };
    const std::vector<std::string> NPET_OPTIONS = {"START", "STOP"};
    SPDLOG_DEBUG("Possible new time correction constant definitions: {}", DEFINITION_OPTIONS);
    switch (Cli::menu("Time correction constant definition", DEFINITION_OPTIONS, false)) {
        case 1: {
            SPDLOG_DEBUG("User selected adjusting raw time correction constant for single NPET");
            const INT NPET_CHOICE = Cli::menu("Select the NPET to adjust the time correction constant for",
                                              NPET_OPTIONS, false);
            SPDLOG_DEBUG("User selected NPET: {}", NPET_CHOICE == 1 ? "START" : "STOP");
            Measurement &const_to_adjust = NPET_CHOICE == 1 ? start_const : stop_const;
            Measurement &const_to_leave = NPET_CHOICE == 1 ? stop_const : start_const;
            NPETComm &npet_to_adjust = NPET_CHOICE == 1 ? start() : stop();
            NPETComm &npet_to_leave = NPET_CHOICE == 1 ? stop() : start();
            const Measurement OLD_CONST = safeExec([&] { return npet_to_adjust.importTimeConstant(); },
                                                   "import_time_constant");
            const_to_adjust = promptRawTimeConstant(OLD_CONST);
            const_to_leave = safeExec([&] { return npet_to_leave.importTimeConstant(); }, "import_time_constant");
            SPDLOG_DEBUG("User specified new time correction constant: {}", const_to_adjust.toString());
            break;
        }
        case 2: {
            SPDLOG_DEBUG("User selected time correction constant synchronization");
            const int AVER_NUM = std::stoi(Cli::prompt("Number of averaging measurements (>=2)", "16"));
            if (AVER_NUM < 2) {
                SPDLOG_ERROR(TIME_CONST_FRAC_INVALID_MEAS_NUM);
                Cli::err(std::string(TIME_CONST_FRAC_INVALID_MEAS_NUM));
                return;
            }
            SPDLOG_DEBUG("User specified number of measurements for averaging: {}", AVER_NUM);
            const auto START_PPS_CHANNEL = promptChannel(2, "the START PPS channel");
            if (!START_PPS_CHANNEL.has_value()) {
                return;
            }
            SPDLOG_DEBUG("User specified START PPS channel: {}", static_cast<int>(START_PPS_CHANNEL.value()));
            const auto STOP_PPS_CHANNEL = promptChannel(2, "the STOP PPS channel");
            if (!STOP_PPS_CHANNEL.has_value()) {
                return;
            }
            SPDLOG_DEBUG("User specified STOP PPS channel: {}", static_cast<int>(STOP_PPS_CHANNEL.value()));
            SPDLOG_DEBUG(TIME_CONST_FRAC_MEAS);
            Cli::echo(std::string(TIME_CONST_FRAC_MEAS));
            // TODO: Add concurrency
            auto bar_start = ProgressBar({.total = AVER_NUM});
            const std::optional<__float128> START_AVE_FRAC = start().getAverageFraction(
                AVER_NUM, START_PPS_CHANNEL.value(), &bar_start);
            if (!START_AVE_FRAC.has_value()) {
                SPDLOG_ERROR(TIME_CONST_FRAC_MEAS_ERR);
                Cli::err(std::string(TIME_CONST_FAILED_TO_SET));
                SPDLOG_DEBUG("Breaking measurement stream ...");
                (void) start().isResponsive(true); // Break measurement stream
                return;
            }
            auto bar_stop = ProgressBar({.total = AVER_NUM});
            const std::optional<__float128> STOP_AVE_FRAC = stop().getAverageFraction(
                AVER_NUM, STOP_PPS_CHANNEL.value(), &bar_stop);
            if (!STOP_AVE_FRAC.has_value()) {
                SPDLOG_ERROR(TIME_CONST_FRAC_MEAS_ERR);
                Cli::err(std::string(TIME_CONST_FAILED_TO_SET));
                SPDLOG_DEBUG("Breaking measurement stream ...");
                (void) start().isResponsive(true); // Break measurement stream
                return;
            }
            Cli::echo(""); // New line after progress bar
            // Use the negative average fractional part to compensate for the offset
            start_const.fracp = -START_AVE_FRAC.value();
            stop_const.fracp = -STOP_AVE_FRAC.value();
            SPDLOG_DEBUG("Measured average fractional part of the START time correction constant: {}",
                         float128ToString(start_const.fracp));
            SPDLOG_DEBUG("Measured average fractional part of the STOP time correction constant: {}",
                         float128ToString(stop_const.fracp));
            SPDLOG_DEBUG("Prompting user for integer part definition logic ...");
            SPDLOG_DEBUG("Possible integer part definition logic options: {}", INT_OPTIONS);
            const int INT_CHOICE = Cli::menu("Integer part setting logic", INT_OPTIONS, false);
            SPDLOG_DEBUG("User selected integer part definition logic: {}", INT_CHOICE);
            if (INT_CHOICE == 4) {
                return;
            }
            SPDLOG_DEBUG("Calculating integer part of the time correction constant with logic id: {}", INT_CHOICE);
            const int CLOCK_TIME = promptTimeConstSeconds();
            // Get the current NPET time
            SPDLOG_DEBUG("Reading current measurement from channel {} to get the NPET time ...",
                         static_cast<int>(START_PPS_CHANNEL.value()));
            const Measurement CURRENT_START_MEASUREMENT = safeExec(
                [&] { return start().readSingleMeasurement(START_PPS_CHANNEL.value()); },
                "read_single_measurement");
            SPDLOG_DEBUG("Current measurement read: {}", CURRENT_START_MEASUREMENT.toString());
            start_const.intp = CLOCK_TIME - CURRENT_START_MEASUREMENT.intp;
            // Get the current NPET time
            SPDLOG_DEBUG("Reading current measurement from channel {} to get the NPET time ...",
                         static_cast<int>(STOP_PPS_CHANNEL.value()));
            const Measurement CURRENT_STOP_MEASUREMENT = safeExec(
                [&] { return stop().readSingleMeasurement(STOP_PPS_CHANNEL.value()); },
                "read_single_measurement");
            SPDLOG_DEBUG("Current measurement read: {}", CURRENT_STOP_MEASUREMENT.toString());
            stop_const.intp = CLOCK_TIME - CURRENT_STOP_MEASUREMENT.intp;
            break;
        }
        case 3:
            SPDLOG_DEBUG("User selected to clear both the time correction constant");
            if (!safeExec([&] { return one_.clearTimeConstant(); }, "clear_time_constant") ||
                !safeExec([&] { return two_.clearTimeConstant(); }, "clear_time_constant")) {
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
    if (start_const.meas_num != -1 || stop_const.meas_num != -1) {
        SPDLOG_ERROR(TIME_CONST_INVALID);
        Cli::err((std::string(TIME_CONST_INVALID)));
        return;
    }
    // TODO: Implement concurrency
    if (!safeExec([&] { return start().exportTimeConstant(start_const); }, "export_time_constant")) {
        SPDLOG_ERROR(TIME_CONST_FAILED_TO_EXPORT);
        Cli::err((std::string(TIME_CONST_FAILED_TO_EXPORT)));
        return;
    }
    if (!safeExec([&] { return stop().exportTimeConstant(stop_const); }, "export_time_constant")) {
        SPDLOG_ERROR(TIME_CONST_FAILED_TO_EXPORT);
        Cli::err((std::string(TIME_CONST_FAILED_TO_EXPORT)));
        return;
    }
    SPDLOG_INFO("{}: {}", START_TIME_CONST_SET, start_const.toString());
    SPDLOG_INFO("{}: {}", STOP_TIME_CONST_SET, stop_const.toString());
    Cli::showStr(std::string(START_TIME_CONST_SET), start_const.toString());
    Cli::showStr(std::string(STOP_TIME_CONST_SET), stop_const.toString());
    // Read sample measurements to see the results
    SPDLOG_DEBUG("Reading sample measurements to show the effect of the new time correction constant ...");
    // TODO: Implement
    // safeExec([&] {
    //              readBatchMeasurements(MeasContext{
    //                  .num_of_meas = 10, .monitor_fn = readerCliSync, .channel = Channel::CH2,
    //              });
    //          },
    //          "read_batch_measurements");
}


void NPETDualCLI::resetCLI() {
    SPDLOG_INFO(DUAL_RESET_INITIATED);
    Cli::echo(std::string(DUAL_RESET_INITIATED), fg::yellow);
    executeBoth([&] { resetNPETSafe(start(), "START"); },
                [&] { resetNPETSafe(stop(), "STOP"); });
    SPDLOG_INFO(DUAL_RESET_COMPLETE);
    Cli::echo(std::string(DUAL_RESET_COMPLETE), fg::green);
} // end of reset_NPET function
