#include <iostream>
#include <string>
#include <fstream>

#include "git_tag.h"
#include "framework/NPET_comm.h"
#include "framework/helper_func.h"
#include "framework/logging.h"
#include "cli/NPET_comm_CLI.h"
#include "cli/cli.h"
#include "cli/helper_func.h"
#include "virtual_machine/vm_main.h"

#include <CLI/CLI.hpp>
#include <spdlog/async.h>
#include <spdlog/spdlog.h>

#include "rang.hpp"


constexpr std::string_view NP_COMM_START_MSG = "NPET communication FW started: ";


///
/// Main CLI function
static int mainCli() {
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


int main(const int argc, char *const*argv) {
    // Initialize logging thread pool first
    spdlog::init_thread_pool(8192, 1); // queue size, 1 background thread
    // The embedded license/manual text is UTF-8; switch the console over so it renders correctly
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    constexpr std::string_view APP_NAME = "NPET communication FW CLI";
    CLI::App app{std::string(APP_NAME)}; // CLI11 app object
    app.description("This program allows communication with the NPET device via command line interface.");
    app.set_version_flag("-v,--version", std::string(APP_NAME) + " (" + BUILD_CONFIG + ") " + GIT_TAG);
    const auto *const RUN = app.add_subcommand("run", "Run the app [default]");
    const auto *const MANUAL = app.add_subcommand("manual", "Show manual");
    const auto *const RESET = app.add_subcommand("reset", "Reset the NPET");
    auto *const VM = app.add_subcommand("virtual", "Run virtual machine NPET which can be used to test the FW");
    VM->set_help_flag("-h,--help", "Show help for the virtual command");
    int vm_ch1_frequency = 100;
    int vm_com_port{};
    VM->add_option("-f,--frequency", vm_ch1_frequency, "Data measurement frequency [Hz] on channel 1")
            ->check(CLI::Range(1, 2500));
    VM->add_option("--com-port", vm_com_port, "COM port number the virtual device connects to")
            ->required();
    const auto *const DATA_PROCESSOR = app.add_subcommand(
        "dp", "Run the NPET data processor, which needs to be installed separately");
    const auto *const LICENSE = app.add_subcommand("license", "Show license information");
    CLI11_PARSE(app, argc, argv);
    int exit_code = 1;
    if (*MANUAL) {
        exit_code = printManual();
    } else if (*RESET) {
        exit_code = resetNpet();
    } else if (*VM) {
        exit_code = launchVm({.com_port = vm_com_port, .ch1_frequency = vm_ch1_frequency});
    } else if (*DATA_PROCESSOR) {
        exit_code = launchDataProcessor();
    } else if (*LICENSE) {
        exit_code = printLicenseInformation();
    } else if (*RUN || app.get_subcommands().empty()) {
        exit_code = mainCli();
        Cli::confirmExit();
    }
    spdlog::shutdown(); // Ensure all logs are flushed before exiting
    return exit_code;
} // end of the main function
