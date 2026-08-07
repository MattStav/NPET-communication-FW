#include "NPET_dual_CLI.h"


constexpr std::string_view NPET_DUAL_OK_RESPONDING = "NPETs communication is OK";
constexpr std::string_view NPET_DUAL_NOT_RESPONDING = "Both NPETs not responding!";
constexpr std::string_view NPET_START_NOT_RESPONDING = "Start NPET not responding!";
constexpr std::string_view NPET_STOP_NOT_RESPONDING = "Stop NPET not responding!";

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
