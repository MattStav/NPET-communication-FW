#include "helper_func.h"

#include <git_tag.h>
#include <license_data.h>
#include <manual_data.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>  // enables formatting of vectors, arrays, etc.

#include "cli.h"
#include "logging.h"
#include "NPET_comm_CLI.h"
#include "ntp_sync.h"

constexpr std::string_view MANUAL_URL = "https://github.com/MattStav/NPET-communication-FW/blob/master/MANUAL.md";
constexpr std::string_view NO_DATA_ERR = "No results to process yet";
constexpr std::string_view DP_ERR = "NPET Data Processor ERROR: Command: {}; Code: {}";
constexpr std::string_view APP_START_MSG = "NPET communication FW started: ";
constexpr std::string_view NO_PORTS = "No available COM ports found";
constexpr std::string_view CHANNEL_INVALID = "Invalid channel number";
constexpr std::string_view BAUD_RATE_CURRENT = "Current baud rate";
constexpr std::string_view BAUD_RATE_OK = "Baud rate set to";
constexpr std::string_view BAUD_RATE_ERR = "Failed to set baud rate";
constexpr std::string_view FW_CURRENT = "Current NPET firmware version";
constexpr std::string_view FW_INVALID = "Invalid choice, firmware version not changed";
constexpr std::string_view SYSTEM_TIME_CURRENT = "Current system time is";


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
    SPDLOG_DEBUG("License information printing initiated ...");
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
/// Data validation allows either a positive integer number or (optionally) -1 for infinity.
/// @param num_to_validate Number to check as string
/// @param ALLOW_NEGATIVE_ONE Whether to allow -1 as a valid input
/// @return The validated number in int format, or -2 if the input is invalid
int numValidation(const std::string &num_to_validate, const bool ALLOW_NEGATIVE_ONE) {
    int number_int = 0;
    SPDLOG_DEBUG("Validating number: {}; allow infinity sentinel: {}", num_to_validate, ALLOW_NEGATIVE_ONE);

    try {
        // Attempt to convert the string to an integer
        number_int = std::stoi(num_to_validate);
        if (const int MIN_VALID_VALUE = ALLOW_NEGATIVE_ONE ? -1 : 0; number_int < MIN_VALID_VALUE) {
            throw std::invalid_argument("Number is out of allowed range");
        }
    } catch (const std::invalid_argument &) {
        SPDLOG_ERROR(INVALID_NUM);
        Cli::err(std::string(INVALID_NUM));
        return INVALID_NUM_SENTINEL;
    }
    SPDLOG_DEBUG("Number {} is valid", number_int);
    return number_int;
} // end of numValidation function


///
/// Prompt the user to select a measurement channel
/// @return Selected channel
std::optional<Channel> promptChannel(const int DEFAULT_CHANNEL, const std::string_view PROMPT_MSG) {
    const std::string CHANNEL_STR = Cli::prompt("Select " + std::string(PROMPT_MSG) + " (1 or 2; 0 to cancel)",
                                                std::to_string(DEFAULT_CHANNEL));
    int channel{};
    try {
        channel = std::stoi(CHANNEL_STR);
        if (channel == 0) {
            return std::nullopt;
        }
        if (channel < 0 || channel >= 3) {
            throw std::invalid_argument(std::string(CHANNEL_INVALID));
        }
    } catch (const std::invalid_argument &) {
        SPDLOG_ERROR(CHANNEL_INVALID);
        Cli::err(std::string(CHANNEL_INVALID));
        return std::nullopt;
    }
    return static_cast<Channel>(channel);
} // end of promptChannel function


///
/// Prompt the user to select a baud rate.
/// @param CURRENT_BAUD_RATE The current baud rate of the NPET communication.
/// @return Selected baud rate
int promptBaudRate(const int CURRENT_BAUD_RATE) {
    const std::vector<std::string> BAUD_RATE_OPTIONS = {
        "115200",
        "230400",
        "576000",
    };
    SPDLOG_DEBUG("Possible new baud rates: {}", BAUD_RATE_OPTIONS);
    SPDLOG_DEBUG("{}: {}", BAUD_RATE_CURRENT, CURRENT_BAUD_RATE);
    Cli::showInt(std::string(BAUD_RATE_CURRENT), CURRENT_BAUD_RATE);
    switch (Cli::menu("New baud rate", BAUD_RATE_OPTIONS, false)) {
        case 1:
            return 115200;
        case 2:
            return 230400;
        case 3:
            return 576000;
        default:
            return CURRENT_BAUD_RATE;
    }
} // end of promptBaudRate function


///
/// Change communication baud rate for the referenced NPET.
/// @param npet The NPETComm reference
/// @param NEW_BAUD_RATE The new baud rate to set for the NPET communication
void setBaudRateSafe(NPETComm &npet, const int NEW_BAUD_RATE) {
    try {
        SPDLOG_WARN("INITIATING BAUD RATE CHANGE!");
        Cli::echo("YOU ARE ABOUT TO CHANGE THE COMMUNICATION BAUD RATE.", fg::yellow, style::bold);
        Cli::echo("DO NOT DISCONNECT THE DEVICE OR CLOSE THE PROGRAM!", fg::yellow, style::bold);
        Cli::echo("IF THIS PROCESS FAILS, RESTART THE DEVICE AND LAUNCH THIS PROGRAM ANEW.", fg::yellow, style::bold);
        Sleep(1000); // Wait for 1 second to let the user read the warning
        if (npet.setBaudRate(NEW_BAUD_RATE)) {
            SPDLOG_DEBUG("{}: {}", BAUD_RATE_OK, NEW_BAUD_RATE);
            Cli::showInt(std::string(BAUD_RATE_OK), NEW_BAUD_RATE);
        } else {
            SPDLOG_ERROR(BAUD_RATE_ERR);
            Cli::err(std::string(BAUD_RATE_ERR));
        }
    } catch (std::runtime_error &e) {
        SPDLOG_ERROR("{}: {}", BAUD_RATE_ERR, e.what());
        Cli::err(BAUD_RATE_ERR.data() + std::string(e.what()));
        Cli::confirmExit();
        throw;
    } // end of try-catch block
}


///
/// Reset all the NPET settings
/// @param npet The NPETComm reference
/// @param designation The NPET name
void resetNPETSafe(NPETComm &npet, const std::string_view designation) {
    if (!safeExec([&] { return npet.clearTimeConstant(); }, std::string(designation) + " clear_time_constant")) {
        SPDLOG_ERROR(TIME_CONST_FAILED_TO_CLEAR);
        Cli::err(std::string(TIME_CONST_FAILED_TO_CLEAR));
    }
    if (!safeExec([&] { return npet.generatePulses(0); }, std::string(designation) + " generate_pulses")) {
        SPDLOG_ERROR(PULSE_GEN_STOP_FAIL);
        Cli::err(std::string(PULSE_GEN_STOP_FAIL));
    }
    if (!safeExec([&] { return npet.setFrequency(); }, std::string(designation) + " set_frequency")) {
        SPDLOG_ERROR(FREQ_RESET_FAIL);
        Cli::err(std::string(FREQ_RESET_FAIL));
    }
    if (!safeExec([&] { return npet.setMeasuredDataFormat(1); }, std::string(designation) + " set_data_format")) {
        SPDLOG_ERROR(DATA_FORMAT_RESET_FAIL);
        Cli::err(std::string(DATA_FORMAT_RESET_FAIL));
    }
    if (!safeExec([&] { return npet.setBaudRate(); }, std::string(designation) + " set_baud_rate")) {
        SPDLOG_ERROR(BAUD_RATE_RESET_FAIL);
        Cli::err(std::string(BAUD_RATE_RESET_FAIL));
    }
}


///
/// Prompt the user to define the NPET internal FW version.
/// @param CURRENT_FW_VERSION The current firmware version of the NPET.
/// @return Selected FW version
FWVersion promptFWVersion(const FWVersion CURRENT_FW_VERSION) {
    const std::vector FW_OPTIONS = {
        std::string(FWVersion(FWVersion::ORIGINAL).getDescription()),
        std::string(FWVersion(FWVersion::AD_REVISION).getDescription()),
    };
    SPDLOG_DEBUG("{}: {}", FW_CURRENT, CURRENT_FW_VERSION.getDescription());
    Cli::showStr(std::string(FW_CURRENT), std::string(CURRENT_FW_VERSION.getDescription()));
    SPDLOG_DEBUG("Selecting new version from: {}", FW_OPTIONS);
    while (true) {
        if (const int USER_CHOICE = Cli::menu("What firmware is your NPET using?", FW_OPTIONS, false);
            USER_CHOICE == FWVersion::ORIGINAL ||
            USER_CHOICE == FWVersion::AD_REVISION ||
            USER_CHOICE == FWVersion::VIRTUAL) {
            const auto SELECTED_FW = FWVersion(USER_CHOICE);
            SPDLOG_DEBUG("User selected firmware: {}", SELECTED_FW.getDescription());
            Cli::echo("Selected firmware: " + std::string(SELECTED_FW.getDescription()));
            return SELECTED_FW;
        }
        SPDLOG_ERROR(FW_INVALID);
        Cli::err(std::string(FW_INVALID));
    }
}


///
/// Prompt the user to define the integer part of the time correction constant
/// @param SEL Time correction constant int part selection logic
/// @return Time correction constant in seconds
int promptTimeConstSeconds(const ConstIntSelectionLogic SEL) {
    int user_choice{};
    int clock_seconds{};

    switch (SEL) {
        case ConstIntSelectionLogic::MANUAL:
            SPDLOG_DEBUG("Logic: Define manually, user will be prompted to enter the target time ...");
            // User defined integer part
            Cli::echo("Enter time of the next 1 Hz measurement in hh:mm:ss format");
            Cli::echo(
                "You will be asked to confirm the values after inputting ss, the calibration will begin once you've confirmed.",
                fg::yellow);
            user_choice = std::stoi(Cli::prompt("Hours", "0"));
            clock_seconds = 3600 * user_choice;
            user_choice = std::stoi(Cli::prompt("Minutes", "0"));
            clock_seconds += 60 * user_choice;
            user_choice = std::stoi(Cli::prompt("Seconds", "0"));
            clock_seconds += user_choice;
            SPDLOG_DEBUG("User specified target time in seconds since midnight: {}", clock_seconds);
            Cli::showInt("Defined clock seconds", clock_seconds);
            SPDLOG_DEBUG("Awaiting final user confirmation for the calibration time");
            if (const bool CONFIRM = Cli::confirm(
                "Confirm by pressing `Enter` when the designated time is about to happen",
                true
            ); !CONFIRM) {
                // Cancel the calibration
                SPDLOG_ERROR("User cancelled the time correction constant integer part calibration");
                return 0;
            }
            SPDLOG_DEBUG("Final confirmation received");
            return clock_seconds;
        case ConstIntSelectionLogic::NTP_SYNC:
            SPDLOG_DEBUG("Logic: Synchronize system time with NTP server ...");
            // Query an NTP server for the current time
            if (!ensureAccurateSystemTime()) {
                Cli::err("Failed to synchronize system time with NTP server");
            }
        // Intentional fallthrough to case IntLogic::SYSTEM_TIME
        case ConstIntSelectionLogic::SYSTEM_TIME: {
            SPDLOG_DEBUG("Logic: Use system time ...");
            // Get the current system time
            Cli::echo("Calculating time correction integer constant from system time");
            const std::time_t NOW = std::time(nullptr);
            std::tm local_time{};
            localtime_s(&local_time, &NOW);
            std::ostringstream oss;
            oss << std::put_time(&local_time, "%H:%M:%S");
            SPDLOG_DEBUG("{}: {}", SYSTEM_TIME_CURRENT, oss.str());
            Cli::showStr(std::string(SYSTEM_TIME_CURRENT), oss.str());
            // Calculate seconds since midnight
            clock_seconds = (local_time.tm_hour * 3600) + (local_time.tm_min * 60) + local_time.tm_sec;
            SPDLOG_DEBUG("Calculated seconds since midnight: {}", clock_seconds);
            return clock_seconds;
        }
        default:
            SPDLOG_ERROR(INVALID_NUM);
            Cli::err(std::string(INVALID_NUM));
            return 0;
    } // end of switch
}
