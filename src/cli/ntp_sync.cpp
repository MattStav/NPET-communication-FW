#include "ntp_sync.h"

#include <string>
#include <spdlog/spdlog.h>

#include "cli.h"
#include "helper_func.h"

constexpr std::string_view PERMISSION_ERR = "PERMISSION ERROR! Admin privileges are required to resync system time with NTP.";
constexpr std::string_view WIN32TIME_START_ERR = "UNKNOWN ERROR! Failed to start Windows Time service (w32time) to resync with NTP.";
constexpr std::string_view PRECHECK_RES = "NTP synchronization is{}possible";
constexpr std::string_view NTP_SYNC_INIT = "Synchronizing system time with NTP server ...";
constexpr std::string_view NTP_SYNC_SUCCESS = "System time successfully synchronized with NTP server.";
constexpr std::string_view NTP_SYNC_ERR = "UNKNOWN ERROR! Failed to synchronize system time with NTP server.";


///
/// Check if a specific Windows service is running.
/// This function uses the `sc query` command to check the status of a service.
/// @return True if the service is running, false otherwise.
bool is_win32time_running() {
    SPDLOG_DEBUG("Checking if Windows Time service (w32time) is running ...");
    const std::string command = "sc query W32time | find \"RUNNING\" >nul 2>&1";
    SPDLOG_DEBUG("Executing command: {}", command);
    const int result = std::system(command.c_str());
    SPDLOG_DEBUG("Service running: {}", result == 0);
    return result == 0; // Returns true if the service is running
} // end of is_service_running function


///
/// Pre-checks before attempting NTP synchronization.
/// Ensures that the user has admin privileges and that the Windows Time service is running.
/// @return True if NTP sync is possible, false otherwise.
bool ntp_precheck() {
    SPDLOG_DEBUG("Performing pre-checks for NTP synchronization ...");
    bool ret{};
    // If not admin, then npt is impossible
    if (!is_user_admin()) {
        SPDLOG_ERROR(PERMISSION_ERR);
        cli::err(PERMISSION_ERR.data());
        cli::echo("Relaunch the app as admin.");
        ret = false;
    // If admin and win32time service is running, ntp is possible
    } else if (is_win32time_running()) ret = true;
    // If admin, attempt to start Windows Time service
    else if (const int start_result = std::system("net start w32time"); start_result == 0) {
        SPDLOG_DEBUG("Windows Time service (w32time) started successfully");
        ret = true;
        Sleep(2000); // Wait for 2 seconds to ensure the service has started
    }
    else {
        SPDLOG_ERROR(WIN32TIME_START_ERR);
        cli::err(WIN32TIME_START_ERR.data());
        ret = false;
    }
    SPDLOG_INFO(PRECHECK_RES, ret ? " " : " NOT ");
    cli::echo(std::format(PRECHECK_RES, ret ? " " : " NOT "));
    return ret;
} // end of ntp_precheck function


///
/// Ensure that the system time is accurate by triggering an NTP sync.
/// This function only works on Windows systems.
/// @return True if the system time was successfully synchronized, false otherwise.
bool ensure_accurate_system_time() {
    if (!ntp_precheck()) return false;
    // Trigger an NTP sync
    SPDLOG_DEBUG(NTP_SYNC_INIT);
    cli::echo(NTP_SYNC_INIT.data());
    if (const int sync_result = std::system("w32tm /resync /force"); sync_result != 0) {
        SPDLOG_ERROR(NTP_SYNC_ERR);
        cli::err(NTP_SYNC_ERR.data());
        return false;
    }
    SPDLOG_INFO(NTP_SYNC_SUCCESS);
    cli::echo(NTP_SYNC_SUCCESS.data(), fg::green);
    return true;
} // end of ensure_accurate_system_time function