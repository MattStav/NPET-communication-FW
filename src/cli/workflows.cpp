#include "workflows.h"

#include <git_tag.h>
#include <spdlog/spdlog.h>

#include "cli.h"
#include "helper_func.h"
#include "logging.h"
#include "NPET_comm_CLI.h"

constexpr std::string_view NP_COMM_START_MSG = "NPET communication FW started: ";


///
/// Main CLI function
int singleNPETMainMenu() {
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
    NPETCommCLI npet_comm{};
    const std::vector<std::string> MAIN_MENU_ITEMS = {
        "Settings",
        "Generate pulses",
        "Take n measurements",
        "Print manual",
        "Launch data processor",
        "Quit program",
    };
    while (npet_comm.isResponsiveCLI()) {
        SPDLOG_DEBUG("NPET is responsive, opening main menu");
        switch (Cli::menu("Main menu", MAIN_MENU_ITEMS)) {
            case 1: // Settings menu
                SPDLOG_DEBUG("Main menu choice: Settings");
                settingsMenu(npet_comm);
                continue;
            case 2: // Generate n pulses
                SPDLOG_DEBUG("Main menu choice: Generate pulses");
                npet_comm.generatePulsesCLI();
                continue;
            case 3: // Read measurements with a specific setting
                SPDLOG_DEBUG("Main menu choice: Read measurements");
                npet_comm.readBatchMeasurementsCLI();
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


int dualNPETMainMenu() {
    return 0;
}
