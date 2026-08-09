#include "ntp_sync.h"

#include <string>
#include <spdlog/spdlog.h>

#include "cli.h"
#include "helper_func.h"

constexpr std::string_view PERMISSION_ERR =
        "PERMISSION ERROR! Admin privileges are required to resync system time with NTP.";
constexpr std::string_view WIN32TIME_START_ERR =
        "UNKNOWN ERROR! Failed to start Windows Time service (w32time) to resync with NTP.";
constexpr std::string_view PRECHECK_RES = "NTP synchronization is{}possible";
constexpr std::string_view NTP_SYNC_INIT = "Synchronizing system time with NTP server ...";
constexpr std::string_view NTP_SYNC_SUCCESS = "System time successfully synchronized with NTP server.";
constexpr std::string_view NTP_SYNC_ERR = "UNKNOWN ERROR! Failed to synchronize system time with NTP server.";


///
/// Check if a specific Windows service is running.
/// This function uses the `sc query` command to check the status of a service.
/// @return True if the service is running, false otherwise.
static bool isWin32timeRunning() {
    SPDLOG_DEBUG("Checking if Windows Time service (w32time) is running ...");
    const std::string COMMAND = "sc query W32time | find \"RUNNING\" >nul 2>&1";
    SPDLOG_DEBUG("Executing command: {}", COMMAND);
    const int RESULT = std::system(COMMAND.c_str()); // NOLINT(bugprone-command-processor, concurrency-mt-unsafe)
    SPDLOG_DEBUG("Service running: {}", RESULT == 0);
    return RESULT == 0; // Returns true if the service is running
} // end of is_service_running function


///
/// Pre-checks before attempting NTP synchronization.
/// Ensures that the user has admin privileges and that the Windows Time service is running.
/// @return True if NTP sync is possible, false otherwise.
static bool ntpPrecheck() {
    SPDLOG_DEBUG("Performing pre-checks for NTP synchronization ...");
    bool ret{};
    // If not admin, then npt is impossible
    if (!isUserAdmin()) {
        SPDLOG_ERROR(PERMISSION_ERR);
        Cli::err(std::string(PERMISSION_ERR));
        Cli::echo("Relaunch the app as admin.");
        ret = false;
        // If admin and win32time service is running, ntp is possible
    } else if (isWin32timeRunning()) {
        ret = true;
        // If admin, attempt to start Windows Time service
    } else if (const int START_RESULT = std::system("net start w32time"); START_RESULT == 0) { // NOLINT(bugprone-command-processor, concurrency-mt-unsafe)
        SPDLOG_DEBUG("Windows Time service (w32time) started successfully");
        ret = true;
        Sleep(2000); // Wait for 2 seconds to ensure the service has started
    } else {
        SPDLOG_ERROR(WIN32TIME_START_ERR);
        Cli::err(std::string(WIN32TIME_START_ERR));
        ret = false;
    }
    SPDLOG_INFO(PRECHECK_RES, ret ? " " : " NOT ");
    Cli::echo(std::format(PRECHECK_RES, ret ? " " : " NOT "));
    return ret;
} // end of ntp_precheck function


bool ensureAccurateSystemTime() {
    if (!ntpPrecheck()) {
        return false;
    }
    // Trigger an NTP sync
    SPDLOG_DEBUG(NTP_SYNC_INIT);
    Cli::echo(std::string(NTP_SYNC_INIT));
    if (const int SYNC_RESULT = std::system("w32tm /resync /force"); SYNC_RESULT != 0) { // NOLINT(bugprone-command-processor, concurrency-mt-unsafe)
        SPDLOG_ERROR(NTP_SYNC_ERR);
        Cli::err(std::string(NTP_SYNC_ERR));
        return false;
    }
    SPDLOG_INFO(NTP_SYNC_SUCCESS);
    Cli::echo(std::string(NTP_SYNC_SUCCESS), fg::green);
    return true;
} // end of ensure_accurate_system_time function
