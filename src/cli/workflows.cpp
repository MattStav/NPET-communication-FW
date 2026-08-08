#include "workflows.h"

#include <spdlog/spdlog.h>

#include "cli.h"
#include "helper_func.h"
#include "logging.h"
#include "NPET_comm_CLI.h"
#include "NPET_dual_CLI.h"


///
/// Settings menu
/// @param npet_comm NPETCommCLI object
static void singleNPETSettingsMenu(NPETCommCLI &npet_comm) {
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


int singleNPETMainMenu() {
    initLogging();
    SPDLOG_INFO("Launching main menu CLI - Single NPET mode");
    printAppIntro();
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
                singleNPETSettingsMenu(npet_comm);
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


///
/// Settings menu
/// @param npet_dual NPETDualCLI object
static void dualNPETSettingsMenu(NPETDualCLI &npet_dual) {
    SPDLOG_DEBUG("Settings menu initiated ...");
    const std::vector<std::string> SETTINGS_MENU_ITEMS = {
        "Communication baud rate",
        "Time correction constants",
        "NPETs FW version",
        "Switch START/STOP designation",
        "Reset NPETs settings",
        "Return to main menu",
    };
    switch (Cli::menu("Settings", SETTINGS_MENU_ITEMS)) {
        case 1: // Change baud rate
            SPDLOG_DEBUG("Settings menu choice: Baud rate");
            npet_dual.setBaudRateCLI();
            return;
        case 2: // Set time constant on NPET
            SPDLOG_DEBUG("Settings menu choice: Time constant");
            // TODO: Implement for dual NPET mode
            throw std::runtime_error("Not implemented for dual NPET mode");
            // npet_dual.setTimeConstantCLI();
            return;
        case 3: // Set FW version
            SPDLOG_DEBUG("Settings menu choice: FW version");
            // TODO: Implement for dual NPET mode
            throw std::runtime_error("Not implemented for dual NPET mode");
            // npet_dual.setFwVerCLI();
            return;
        case 4:
            SPDLOG_DEBUG("Settings menu choice: Switch START/STOP designation");
            npet_dual.switchStartStopCLI();
            return;
        case 5: // Reset NPET
            SPDLOG_DEBUG("Settings menu choice: Reset NPET");
            // TODO: Implement for dual NPET mode
            throw std::runtime_error("Not implemented for dual NPET mode");
            // npet_dual.resetCLI();
        default: ;
    } // end of switch
} // end of menu_settings function


int dualNPETMainMenu() {
    initLogging();
    SPDLOG_INFO("Launching main menu CLI - Dual NPET mode");
    printAppIntro();
    // Confirm that both NPETs are ready to connect
    if (!Cli::confirm("Please confirm that both NPETs are configured to 115200 Baud rate and 8N1 mode!", true)) {
        SPDLOG_DEBUG("User did not confirm NPET configuration, exiting program");
        return 1;
    }
    SPDLOG_DEBUG("User confirmed NPET configuration, proceeding with initialization");
    SPDLOG_INFO("Initializing NPETDual communication framework in CLI mode");
    NPETDualCLI npet_dual{};
    const std::vector<std::string> MAIN_MENU_ITEMS = {
        "Settings",
        "Take n measurements",
        "Print manual",
        "Launch data processor",
        "Quit program",
    };
    while (npet_dual.bothResponsiveCLI()) {
        SPDLOG_DEBUG("Both NPETs are responsive, opening main menu");
        switch (Cli::menu("Main menu", MAIN_MENU_ITEMS)) {
            case 1: // Settings menu
                SPDLOG_DEBUG("Main menu choice: Settings");
                dualNPETSettingsMenu(npet_dual);
                continue;
            case 2: // Read measurements with a specific setting
                SPDLOG_DEBUG("Main menu choice: Read measurements");
                npet_dual.readBatchMeasurementsCLI();
                continue;
            case 3: // Print the manual
                SPDLOG_DEBUG("Main menu choice: Print manual");
                printManual();
                continue;
            case 4: // Launch data processor
                SPDLOG_DEBUG("Main menu choice: Launch data processor");
                launchDataProcessor();
                continue;
            case 5: // Quit the program
                SPDLOG_DEBUG("Main menu choice: Quit program");
                // Class destructors handle cleanup
                return 0;
            default: ;
        } // end of switch
    } // end of infinite while loop
    SPDLOG_ERROR("NPET is not responsive, exiting program");
    return 1;
}
