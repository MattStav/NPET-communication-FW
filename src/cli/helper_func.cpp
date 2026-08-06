#include "helper_func.h"

#include <license_data.h>
#include <manual_data.h>
#include <spdlog/spdlog.h>

#include "cli.h"
#include "logging.h"

constexpr std::string_view MANUAL_URL = "https://github.com/MattStav/NPET-communication-FW/blob/master/MANUAL.md";
constexpr std::string_view NO_DATA_ERR = "No results to process yet";
constexpr std::string_view DP_ERR = "NPET Data Processor ERROR: Command: {}; Code: {}";


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
