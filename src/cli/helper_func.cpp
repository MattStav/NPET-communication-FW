#include "helper_func.h"

#include <license_data.h>
#include <manual_data.h>
#include <spdlog/spdlog.h>

#include "cli.h"

constexpr std::string_view MANUAL_URL = "https://github.com/MattStav/NPET-communication-FW/blob/master/MANUAL.md";
constexpr std::string_view NO_DATA_ERR = "No results to process yet";
constexpr std::string_view DP_ERR = "NPET Data Processor ERROR: Command: {}; Code: {}";
constexpr std::string_view NP_COMM_START_MSG = "NPET communication FW started: ";


///
/// Print the manual into the console.
/// @return Exit code 0
int printManual() {
    SPDLOG_DEBUG("Manual printing initiated ...");
    cli::echo("To view the fully formatted latest manual see here:");
    cli::echo(std::string(MANUAL_URL), fg::blue, style::bold);
    cli::echo("If you cannot access the site, you may also print the unformatted manual into console.");
    cli::echo("WARNING! The manual is quite long and its format will be broken.", fg::yellow);
    if (cli::confirm("Confirm to print the manual")) {
        SPDLOG_DEBUG("Manual printing confirmed, printing manual");
        std::cout << '\n'; // Empty line
        cli::echo(manual_text);
        SPDLOG_DEBUG("Manual printing completed");
    } else {
        SPDLOG_DEBUG("Manual printing cancelled by user");
    }
    return 0;
} // end of print_manual function


///
/// Print the license information into the console.
int printLicenseInformation() {
    cli::echo("NPET communication FW License Information:\n\n");
    cli::echo(license_text);
    cli::echo(notice_text);
    cli::echo("\nThird-party software licenses:\n\n");
    cli::echo(third_party_notices_text);
    return 0;
} // end of print_license_information function


///
/// Reset NPET into default settings.
/// @return Exit code 0
int resetNpet() {
    SPDLOG_DEBUG("NPET reset initiated ...");
    cli::echo("Resetting NPET to default settings", fg::blue, style::bold, true);
    NPET_comm_CLI npet_comm{};
    npet_comm.reset_CLI();
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
    int ret_code{};
    cli::echo("Now launching external data processor");
    for (const auto &command: DP_COMMANDS) {
        SPDLOG_DEBUG("Launching command: {}", command);
        ret_code = system(command.c_str()); // NOLINT(bugprone-command-processor, concurrency-mt-unsafe)
        if (ret_code == 0) {
            SPDLOG_DEBUG("Data processor terminated");
            return 0;
        }
        if (ret_code == 10) {
            SPDLOG_ERROR(NO_DATA_ERR);
            cli::err(std::string(NO_DATA_ERR));
            return 1;
        }
        SPDLOG_ERROR(DP_ERR, command, ret_code);
        cli::err(std::format(DP_ERR, command, ret_code));
    } // end of for loop
    return 1;
} // end of launch_data_processor function


///
/// Main CLI function
static int singleNPETMainMenu() {
    initLogging();
    SPDLOG_INFO("Launching main menu CLI");
    Cli::echo(std::string(NP_COMM_START_MSG), fg::blue, style::bold, false);
    Cli::echo(BUILD_CONFIG " " GIT_TAG, fg::yellow);
    SPDLOG_INFO("{} {} {}", NP_COMM_START_MSG, BUILD_CONFIG, GIT_TAG);
    Cli::echo("If you have any questions please refer to the manual, which should be provided with the program.");
    Cli::echo("If manual wasn't provided, you can access it from the main menu, "
              "or it can be opened by calling this program from cmd with the 'manual' command.", fg::yellow);
    std::cout << '\n'; // Empty line.
    Cli::showStr("Log path", getLogPath().string()); // Already automatically included in logs
    // Confirm that the NPET is ready to connect
    if (!Cli::confirm("Please confirm that the NPET is configured to 115200 Baud rate and 8N1 mode!", true)) {
        SPDLOG_DEBUG("User did not confirm NPET configuration, exiting program");
        return 1;
    }
    SPDLOG_DEBUG("User confirmed NPET configuration, proceeding with initialization");
    SPDLOG_INFO("Initializing NPET communication framework in CLI mode");
    NPET_comm_CLI npet_comm{};
    const std::vector<std::string> MAIN_MENU_ITEMS = {
        "Settings",
        "Generate pulses",
        "Take n measurements",
        "Print manual",
        "Launch data processor",
        "Quit program",
    };
    while (npet_comm.is_responsive_CLI()) {
        SPDLOG_DEBUG("NPET is responsive, opening main menu");
        switch (Cli::menu("Main menu", MAIN_MENU_ITEMS)) {
            case 1: // Settings menu
                SPDLOG_DEBUG("Main menu choice: Settings");
                settingsMenu(npet_comm);
                continue;
            case 2: // Generate n pulses
                SPDLOG_DEBUG("Main menu choice: Generate pulses");
                npet_comm.generate_pulses_CLI();
                continue;
            case 3: // Read measurements with a specific setting
                SPDLOG_DEBUG("Main menu choice: Read measurements");
                npet_comm.read_batch_measurements_CLI();
                continue;
            case 4: // Print the manual
                SPDLOG_DEBUG("Main menu choice: Print manual");
                printManual();
                continue;
            case 5: // Launch data processor
                SPDLOG_DEBUG("Main menu choice: Launch data processor");
                launchDataProcessor();
                continue;
            case 6: // Quit the program
                SPDLOG_DEBUG("Main menu choice: Quit program");
                // Class destructors handle cleanup
                return 0;
            default: ;
        } // end of switch
    } // end of infinite while loop
    SPDLOG_ERROR("NPET is not responsive, exiting program");
    return 1;
} // end of the main function



///
/// Settings menu
/// @param npet_comm NPET_comm_CLI object
void settingsMenu(NPET_comm_CLI &npet_comm) {
    SPDLOG_DEBUG("Settings menu initiated ...");
    const std::vector<std::string> SETTINGS_MENU_ITEMS = {
        "Communication baud rate",
        "Time correction constant",
        "NPET FW version",
        "Reset NPET settings",
        "Return to main menu",
    };
    switch (cli::menu("Settings", SETTINGS_MENU_ITEMS)) {
        case 1: // Change baud rate
            SPDLOG_DEBUG("Settings menu choice: Baud rate");
            npet_comm.set_baud_rate_CLI();
            return;
        case 2: // Set time constant on NPET
            SPDLOG_DEBUG("Settings menu choice: Time constant");
            npet_comm.set_time_constant_CLI();
            return;
        case 3: // Set FW version
            SPDLOG_DEBUG("Settings menu choice: FW version");
            npet_comm.set_FW_ver_CLI();
            return;
        case 4: // Reset NPET
            SPDLOG_DEBUG("Settings menu choice: Reset NPET");
            npet_comm.reset_CLI();
        default: ;
    } // end of switch
} // end of menu_settings function
