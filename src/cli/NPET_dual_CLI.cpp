#include "NPET_dual_CLI.h"


constexpr std::string_view NPET_DUAL_OK_RESPONDING = "NPETs communication is OK";
constexpr std::string_view NPET_DUAL_NOT_RESPONDING = "Both NPETs not responding!";
constexpr std::string_view NPET_START_NOT_RESPONDING = "Start NPET not responding!";
constexpr std::string_view NPET_STOP_NOT_RESPONDING = "Stop NPET not responding!";

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
        const int COM_PORT = selectComPortCli(autoselect, excluded_ports);
        if (!openCommSafe(npet, COM_PORT, ERROR_MSG)) {
            continue;
        }
        npet.detectFWVer();
        if (!confirmNPETSelection(npet, designation)) {
            npet.closeCommunication();
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


///
/// Open serial communication with both NPETs.
void NPETDualCLI::openCommunicationCLI() {
    SPDLOG_INFO("Opening both NPETs communication with CLI");
    const int START_PORT = openCommLoop(start_, "START", NPET_START_NOT_RESPONDING);
    openCommLoop(stop_, "STOP", NPET_STOP_NOT_RESPONDING, START_PORT);
    SPDLOG_INFO("Both NPETs communication opened successfully");
} // end of open_NPET_communication function


///
/// Checks if both NPET devices are connected to the specified COM ports and responsive.
/// Several attempts are made with a delay between each attempt.
/// Prints the status to the CLI.
/// @return True if both NPETs are connected and responsive, otherwise false.
bool NPETDualCLI::bothResponsiveCLI() {
    constexpr int MAX_ATTEMPTS = 5;
    constexpr int RETRY_DELAY_MS = 1000;
    bool start_res{false};
    bool stop_res{false};

    SPDLOG_DEBUG("Checking NPETs responsiveness; max attempts: {}; retry delay: {} ms", MAX_ATTEMPTS, RETRY_DELAY_MS);
    for (int attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
        SPDLOG_DEBUG("Responsiveness check attempt: {}", attempt + 1);
        if (!start_res) {
            start_res = safeExec([&] { return start_.isResponsive(); }, "is_start_responsive");
        }
        if (!stop_res) {
            stop_res = safeExec([&] { return stop_.isResponsive(); }, "is_stop_responsive");
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
            start_.purgePort();
            stop_.purgePort();
        } else if (!start_res) {
            SPDLOG_ERROR(NPET_START_NOT_RESPONDING);
            Cli::err(std::string(NPET_START_NOT_RESPONDING));
            start_.purgePort();
        } else {
            SPDLOG_ERROR(NPET_STOP_NOT_RESPONDING);
            Cli::err(std::string(NPET_STOP_NOT_RESPONDING));
            stop_.purgePort();
        }
        Sleep(RETRY_DELAY_MS);
    } // end of for loop
    SPDLOG_ERROR("NPETs are not responsive after {} attempts", MAX_ATTEMPTS);
    return false;
} // end of is_NPET_connected_CLI function


///
/// CLI wrapper for the reading batch measurements from both NPETs.
/// Asks the user for the number of measurements, channel to read from, display, and save options.
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
    // TODO: Add monitoring
    // const std::string MONITOR_STR = Cli::prompt("Measurement monitoring (0 - None, 1 - Basic, 2 - Advanced, 3 - Sync)",
                                                // "1");
    int monitor{};
    // try {
        // monitor = std::stoi(MONITOR_STR);
    // } catch (const std::invalid_argument &) {
        // SPDLOG_ERROR(MONITOR_FN_INVALID);
        // Cli::err(std::string(MONITOR_FN_INVALID));
        // return;
    // } // end of try-catch block
    // SPDLOG_DEBUG("User specified measurement monitoring: {}", monitor);
    std::function<void(MeasReader &, const MeasContext &, const Measurement &)>
            monitor_fn;
    switch (monitor) {
        // case 1: monitor_fn = readerCliBasic;
            // break;
        // case 2: monitor_fn = readerCliAdvanced;
            // break;
        // case 3: monitor_fn = readerCliSync;
            // break;
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
