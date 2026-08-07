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
    npet.readSingleMeasurement(Channel::CH2);
    const bool RET = Cli::confirm("Confirm that the " + designation + " NPET is currently set to channel 2?", true);
    npet.readSingleMeasurement(Channel::CH1);
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

    SPDLOG_INFO("Opening {} NPET communication with CLI, max attempts: {}", designation, MAX_ATTEMPTS);
    for (int i = 0; i < MAX_ATTEMPTS; i++) {
        // After 2 failed attempt, disable autoselect
        if (i == 2) {
            autoselect = false;
        }
        SPDLOG_DEBUG("Attempt {} to open {} NPET communication", i + 1, designation);
        Cli::echo("Select the COM port number for the " + designation + " NPET", fg::gray, style::bold);
        const int COM_PORT = selectComPortCli(autoselect, std::vector{EXCLUDED_PORT});
        if (!openCommSafe(npet, COM_PORT, ERROR_MSG)) {
            continue;
        }
        npet.detectFWVer();
        if (!confirmNPETSelection(npet, designation)) {
            npet.closeCommunication();
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
