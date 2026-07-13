#include <iostream>
#include <string>
#include <fstream>

#include "manual_data.h"
#include "license_data.h"
#include "git_tag.h"
#include "framework/NPET_comm.h"
#include "framework/helper_func.h"
#include "framework/logging.h"
#include "cli/NPET_comm_CLI.h"
#include "cli/cli.h"
#include "virtual_machine/virtual_machine.h"

#include <CLI/CLI.hpp>
#include <spdlog/async.h>
#include <spdlog/spdlog.h>
#include "rang.hpp"


using namespace rang;
constexpr std::string_view GITHUB_URL = "https://github.com/MattStav/NPET-communication-FW/blob/master";
const std::string NPET_COMMAND = "npet-dp --data-path " + (USER_FILES / OUTPUT_DIR_NAME).string();
const std::string NPET_COMMAND_BACKUP = "py -m NPET_DP --data-path " + (USER_FILES / OUTPUT_DIR_NAME).string();
const std::vector DP_COMMANDS = {NPET_COMMAND, NPET_COMMAND_BACKUP};
const std::string MANUAL_URL = std::string(GITHUB_URL) + "/MANUAL.md";
constexpr std::string_view NO_DATA_ERR = "No results to process yet";
constexpr std::string_view DP_ERR = "NPET Data Processor ERROR: Command: {}; Code: {}";
constexpr std::string_view NP_COMM_START_MSG = "NPET communication FW started: ";


///
/// Print the manual into the console.
/// @return Exit code 0
int print_manual() {
    SPDLOG_DEBUG("Manual printing initiated ...");
    cli::echo("To view the fully formatted latest manual see here:");
    cli::echo(MANUAL_URL, fg::blue, style::bold);
    cli::echo("If you cannot access the site, you may also print the unformatted manual into console.");
    cli::echo("WARNING! The manual is quite long and its format will be broken.", fg::yellow);
    if (cli::confirm("Confirm to print the manual")) {
        SPDLOG_DEBUG("Manual printing confirmed, printing manual");
        std::cout << std::endl; // Empty line
        cli::echo(manual_text);
        SPDLOG_DEBUG("Manual printing completed");
    } else SPDLOG_DEBUG("Manual printing cancelled by user");
    return 0;
} // end of print_manual function


///
/// Print the license information into the console.
int print_license_information() {
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
int reset_NPET() {
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
int launch_data_processor() {
    SPDLOG_DEBUG("Launching external data processor ...");
    int ret_code{};
    cli::echo("Now launching external data processor");
    for (const auto &command: DP_COMMANDS) {
        SPDLOG_DEBUG("Launching command: {}", command);
        ret_code = system(command.c_str());
        if (ret_code == 0) {
            SPDLOG_DEBUG("Data processor terminated");
            return 0;
        }
        if (ret_code == 10) {
            SPDLOG_ERROR(NO_DATA_ERR);
            cli::err(NO_DATA_ERR.data());
            return 1;
        }
        SPDLOG_ERROR(DP_ERR, command, ret_code);
        cli::err(std::format(DP_ERR, command, ret_code));
    } // end of for loop
    return 1;
} // end of launch_data_processor function


///
/// Settings menu
/// @param npet_comm NPET_comm_CLI object
void settings_menu(NPET_comm_CLI &npet_comm) {
    SPDLOG_DEBUG("Settings menu initiated ...");
    const std::vector<std::string> settings_menu_items = {
        "Communication baud rate",
        "Time correction constant",
        "NPET FW version",
        "Reset NPET settings",
        "Return to main menu",
    };
    switch (cli::menu("Settings", settings_menu_items)) {
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


///
/// Main CLI function
int main_cli() {
    init_logging();
    SPDLOG_INFO("Launching main menu CLI");
    cli::echo(NP_COMM_START_MSG.data(), fg::blue, style::bold, false);
    cli::echo(GIT_TAG, fg::yellow);
    SPDLOG_INFO("{}{}", NP_COMM_START_MSG, GIT_TAG);
    cli::echo("If you have any questions please refer to the manual, which should be provided with the program.");
    cli::echo("If manual wasn't provided, you can access it from the main menu, "
              "or it can be opened by calling this program from cmd with the 'manual' command.", fg::yellow);
    std::cout << std::endl; // Empty line.
    cli::show_str("Log path", get_log_path().string()); // Already automatically included in logs
    // Confirm that the NPET is ready to connect
    if (!cli::confirm("Please confirm that the NPET is configured to 115200 Baud rate and 8N1 mode!", true)) {
        SPDLOG_DEBUG("User did not confirm NPET configuration, exiting program");
        return 1;
    }
    SPDLOG_DEBUG("User confirmed NPET configuration, proceeding with initialization");
    SPDLOG_INFO("Initializing NPET communication framework in CLI mode");
    NPET_comm_CLI npet_comm{};
    const std::vector<std::string> main_menu_items = {
        "Settings",
        "Generate pulses",
        "Take n measurements",
        "Print manual",
        "Launch data processor",
        "Quit program"
    };
    while (npet_comm.is_responsive_CLI()) {
        SPDLOG_DEBUG("NPET is responsive, opening main menu");
        switch (cli::menu("Main menu", main_menu_items)) {
            case 1: // Settings menu
                SPDLOG_DEBUG("Main menu choice: Settings");
                settings_menu(npet_comm);
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
                print_manual();
                continue;
            case 5: // Launch data processor
                SPDLOG_DEBUG("Main menu choice: Launch data processor");
                launch_data_processor();
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


int main(const int argc, char **argv) {
    // Initialize logging thread pool first
    spdlog::init_thread_pool(8192, 1); // queue size, 1 background thread
    // The embedded license/manual text is UTF-8; switch the console over so it renders correctly
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    CLI::App app{"NPET communication FW CLI"}; // CLI11 app object
    app.description("This program allows communication with the NPET device via command line interface.");
    const auto run = app.add_subcommand("run", "Run the app [default]");
    const auto manual = app.add_subcommand("manual", "Show manual");
    const auto reset = app.add_subcommand("reset", "Reset the NPET");
    const auto virtual_machine = app.add_subcommand("virtual_machine", "Run virtual machine NPET");
    const auto data_processor = app.add_subcommand(
        "dp", "Run the NPET data processor, which needs to be installed separately");
    const auto license = app.add_subcommand("license", "Show license information");
    CLI11_PARSE(app, argc, argv);
    int exit_code = 1;
    if (*manual) exit_code = print_manual();
    else if (*reset) exit_code = reset_NPET();
    else if (*virtual_machine) exit_code = launch_vm();
    else if (*data_processor) exit_code = launch_data_processor();
    else if (*license) exit_code = print_license_information();
    else if (*run || app.get_subcommands().empty()) {
        exit_code = main_cli();
        cli::confirm_exit();
    }
    spdlog::shutdown(); // Ensure all logs are flushed before exiting
    return exit_code;
} // end of the main function
