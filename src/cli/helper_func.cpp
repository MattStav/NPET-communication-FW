#include "helper_func.h"

#include <git_tag.h>
#include <license_data.h>
#include <manual_data.h>
#include <spdlog/spdlog.h>

#include "cli.h"
#include "logging.h"
#include "NPET_comm_CLI.h"

constexpr std::string_view MANUAL_URL = "https://github.com/MattStav/NPET-communication-FW/blob/master/MANUAL.md";
constexpr std::string_view NO_DATA_ERR = "No results to process yet";
constexpr std::string_view DP_ERR = "NPET Data Processor ERROR: Command: {}; Code: {}";
constexpr std::string_view APP_START_MSG = "NPET communication FW started: ";
constexpr std::string_view NO_PORTS = "No available COM ports found";


void printAppIntro() {
    Cli::echo(std::string(APP_START_MSG), fg::blue, style::bold, false);
    Cli::echo(BUILD_CONFIG " " GIT_TAG, fg::yellow);
    SPDLOG_INFO("{} {} {}", APP_START_MSG, BUILD_CONFIG, GIT_TAG);
    Cli::echo("If you have any questions please refer to the manual, which should be provided with the program.");
    Cli::echo("If manual wasn't provided, you can access it from the main menu, "
              "or it can be opened by calling this program from terminal with the 'manual' command.", fg::yellow);
    std::cout << '\n'; // Empty line.
    Cli::showStr("Log path", getLogPath().string()); // Already automatically included in logs
}

///
/// Print the manual into the console.
/// @return Exit code 0
int printManual() {
    SPDLOG_DEBUG("Manual printing initiated ...");
    Cli::echo("To view the fully formatted latest manual see here:");
    Cli::echo(std::string(MANUAL_URL), fg::blue, style::bold);
    Cli::echo("If you cannot access the site, you may also print the unformatted manual into console.");
    Cli::echo("WARNING! The manual is quite long and its format will be broken.", fg::yellow);
    if (Cli::confirm("Confirm to print the manual")) {
        SPDLOG_DEBUG("Manual printing confirmed, printing manual");
        std::cout << '\n'; // Empty line
        Cli::echo(manual_text);
        SPDLOG_DEBUG("Manual printing completed");
    } else {
        SPDLOG_DEBUG("Manual printing cancelled by user");
    }
    return 0;
} // end of print_manual function


///
/// Print the license information into the console.
int printLicenseInformation() {
    Cli::echo("NPET communication FW License Information:\n\n");
    Cli::echo(license_text);
    Cli::echo(notice_text);
    Cli::echo("\nThird-party software licenses:\n\n");
    Cli::echo(third_party_notices_text);
    return 0;
} // end of print_license_information function


///
/// Reset NPET into default settings.
/// @return Exit code 0
int resetNpetStandalone() {
    SPDLOG_DEBUG("NPET reset initiated ...");
    Cli::echo("Resetting NPET to default settings", fg::blue, style::bold, true);
    NPETCommCLI npet_comm{};
    npet_comm.resetCLI();
    SPDLOG_DEBUG("NPET reset completed");
    return 0;
} // end of reset_NPET function


///
/// Launch the NPET data processor which needs to be installed separately.
/// The data processor is a Python package that processes the raw measurement data and generates plots and reports.
int launchDataProcessor() {
    SPDLOG_DEBUG("Launching external data processor ...");
    static const std::vector DP_COMMANDS = {
        "npet-dp --data-path " + (USER_FILES / OUTPUT_DIR_NAME).string(),
        "py -m NPET_DP --data-path " + (USER_FILES / OUTPUT_DIR_NAME).string(),
    };
    Cli::echo("Now launching external data processor");
    for (const auto &command: DP_COMMANDS) {
        SPDLOG_DEBUG("Launching command: {}", command);
        const int RET_CODE = system(command.c_str()); // NOLINT(bugprone-command-processor, concurrency-mt-unsafe)
        if (RET_CODE == 0) {
            SPDLOG_DEBUG("Data processor terminated");
            return 0;
        }
        if (RET_CODE == 10) {
            SPDLOG_ERROR(NO_DATA_ERR);
            Cli::err(std::string(NO_DATA_ERR));
            return 1;
        }
        SPDLOG_ERROR(DP_ERR, command, RET_CODE);
        Cli::err(std::format(DP_ERR, command, RET_CODE));
    } // end of for loop
    return 1;
} // end of launch_data_processor function

///
/// Lists available COM ports and prompts the user to select one.
/// If autoselect is true and only one COM port is available, it will be selected automatically.
/// Ports listed in EXCLUDED_PORTS are dropped from the selection before it is shown.
/// WARNING: This function does not check if the selected COM port is valid.
/// @param AUTOSELECT If true, automatically select the COM port if only one is available.
/// @param EXCLUDED_PORTS COM port numbers to exclude from the selection (e.g. {5, 8}).
/// @throws runtime_error if no COM ports are found.
/// @returns Selected COM port number (e.g. 8 for COM8).
int selectComPortCli(const bool AUTOSELECT, const std::vector<int> &EXCLUDED_PORTS) {
    SPDLOG_DEBUG("Selecting COM port with autoselect: {}", AUTOSELECT);
    int selected_cp{};
    std::vector<std::string> com_ports{};

    // Get available com ports, dropping any that are excluded
    try {
        com_ports = getComPorts(getWin32Api(), EXCLUDED_PORTS);
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
        selected_cp = extractComPortNumber(com_ports.at(0));
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
/// Open communication for the referenced NPET on the provided NPET.
/// Errors are handled internally and False is returned if any errors are encountered.
/// @param npet The NPETComm reference
/// @param COM_PORT COM port number
/// @param ERROR_MSG Error message in case of an error
/// @return True if communication was successfully opened, False otherwise
bool openCommSafe(NPETComm &npet, const int COM_PORT, const std::string_view ERROR_MSG) {
    Cli::showInt("Opening the NPET communication on COM", COM_PORT);
    if (COM_PORT < 1) {
        SPDLOG_ERROR(INVALID_COM_PORT);
        Cli::err(std::string(INVALID_COM_PORT));
        return false;
    }
    try {
        npet.openCommunication(COM_PORT, DEFAULT_BAUD_RATE);
    } catch (std::exception &e) {
        SPDLOG_ERROR(FAILED_OPEN_COM_PORT, e.what());
        Cli::err(std::format(FAILED_OPEN_COM_PORT, e.what()));
        return false;
    } // end of try-catch block
    if (!safeExec([&] { return npet.isResponsive(); }, "is_responsive")) {
        Cli::echo("COM port opened successfully", fg::yellow);
        SPDLOG_ERROR(ERROR_MSG);
        Cli::err(std::string(ERROR_MSG));
        npet.closeCommunication();
        return false;
    }
    return true;
}

///
/// Settings menu
/// @param npet_comm NPET_comm_CLI object
void settingsMenu(NPETCommCLI &npet_comm) {
    SPDLOG_DEBUG("Settings menu initiated ...");
    const std::vector<std::string> SETTINGS_MENU_ITEMS = {
        "Communication baud rate",
        "Time correction constant",
        "NPET FW version",
        "Reset NPET settings",
        "Return to main menu",
    };
    switch (Cli::menu("Settings", SETTINGS_MENU_ITEMS)) {
        case 1: // Change baud rate
            SPDLOG_DEBUG("Settings menu choice: Baud rate");
            npet_comm.setBaudRateCLI();
            return;
        case 2: // Set time constant on NPET
            SPDLOG_DEBUG("Settings menu choice: Time constant");
            npet_comm.setTimeConstantCLI();
            return;
        case 3: // Set FW version
            SPDLOG_DEBUG("Settings menu choice: FW version");
            npet_comm.setFwVerCLI();
            return;
        case 4: // Reset NPET
            SPDLOG_DEBUG("Settings menu choice: Reset NPET");
            npet_comm.resetCLI();
        default: ;
    } // end of switch
} // end of menu_settings function
