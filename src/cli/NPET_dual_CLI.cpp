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
/// Open serial communication with both NPETs.
/// TODO
void NPETDualCLI::openCommunicationCLI() {
    constexpr int MAX_ATTEMPTS = 3;

    SPDLOG_INFO("Opening START NPET communication with CLI, max attempts: {}", MAX_ATTEMPTS);
    for (int i = 0; i < MAX_ATTEMPTS; i++) {
        SPDLOG_DEBUG("Attempt {} to open NPETs communication", i + 1);
        Cli::echo("Select the COM port number for the START NPET", fg::gray, style::bold);
        const int COM_PORT = selectComPortCli(false);
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
        // TODO: Add confirmation
        // TODO: Add stop npet
        SPDLOG_INFO("NPET communication opened successfully");
        return;
    } // end of for loop
    SPDLOG_ERROR(FAILED_OPEN_COM_PORT_MAX_ATTEMPT, MAX_ATTEMPTS);
    Cli::err(std::format(FAILED_OPEN_COM_PORT_MAX_ATTEMPT, MAX_ATTEMPTS));
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
            start_res = safeExec([&] { return isStartResponsive(); }, "is_start_responsive");
        }
        if (!stop_res) {
            stop_res = safeExec([&] { return isStopResponsive(); }, "is_stop_responsive");
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
            purgeStartPort();
            purgeStopPort();
        } else if (!start_res) {
            SPDLOG_ERROR(NPET_START_NOT_RESPONDING);
            Cli::err(std::string(NPET_START_NOT_RESPONDING));
            purgeStartPort();
        } else {
            SPDLOG_ERROR(NPET_STOP_NOT_RESPONDING);
            Cli::err(std::string(NPET_STOP_NOT_RESPONDING));
            purgeStopPort();
        }
        Sleep(RETRY_DELAY_MS);
    } // end of for loop
    SPDLOG_ERROR("NPETs are not responsive after {} attempts", MAX_ATTEMPTS);
    return false;
} // end of is_NPET_connected_CLI function
