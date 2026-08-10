#ifndef NPET_COMM_FW_HELPER_FUNC_H
#define NPET_COMM_FW_HELPER_FUNC_H
#include <vector>

#include "NPET_comm.h"
// Forward declaration
class NPETCommCLI;

constexpr std::string_view APP_NAME = "NPET communication FW CLI";
constexpr int INVALID_NUM_SENTINEL = -2;
constexpr std::string_view INVALID_COM_PORT = "Invalid COM port number";
constexpr std::string_view FAILED_OPEN_COM_PORT = "Failed to open the COM port: {}";
constexpr std::string_view FAILED_OPEN_COM_PORT_MAX_ATTEMPT = "Failed to open NPET communication after {} attempts";
constexpr std::string_view INVALID_NUM = "Invalid input. Number out of allowed range";
constexpr std::string_view PULSE_GEN_STOP_FAIL = "Failed to stop pulse generation";
constexpr std::string_view FREQ_RESET_FAIL = "Failed to reset the pulse generation frequency";
constexpr std::string_view BAUD_RATE_RESET_FAIL = "Failed to reset baud rate to default";
constexpr std::string_view BAUD_RATE_ALREADY_SET = "New baud rate is the same as the current one";
constexpr std::string_view DATA_FORMAT_RESET_FAIL = "Failed to reset the data format";
constexpr std::string_view TIME_CONST_FAILED_TO_CLEAR = "Failed to clear the time correction constant saved in NPET";
constexpr std::string_view TIME_CONST_FAILED_TO_SET = "Failed to define raw time constant: {}";
constexpr std::string_view TIME_CONST_FRAC_INVALID_MEAS_NUM = "Invalid number of averaging measurements";
constexpr std::string_view TIME_CONST_CLEAR_OK = "Time correction constant cleared";
constexpr std::string_view MONITOR_FN_INVALID = "Invalid monitor function";
constexpr std::string_view TIME_CONST_FRAC_MEAS = "Measuring average fractional part of the time correction constant";
constexpr std::string_view TIME_CONST_FRAC_MEAS_ERR = "Error occurred during fraction measurement";
constexpr std::string_view TIME_CONST_INVALID = "Time correction constant invalid";
constexpr std::string_view TIME_CONST_FAILED_TO_EXPORT = "Failed to export the time constant to NPET";

enum class ConstIntSelectionLogic : std::uint8_t {
    MANUAL = 1,
    SYSTEM_TIME = 2,
    NTP_SYNC = 3,
};

const std::vector<std::string> INT_OPTIONS = {
    "User defined",
    "System time",
    "System time with NTP sync (Admin required)",
    "Cancel",
};

///
/// Print the manual into the console.
/// @return Exit code 0
int printManual();

///
/// Print the license information into the console.
int printLicenseInformation();

///
/// Reset NPET into default settings.
/// @return Exit code 0
int resetNpetStandalone();

///
/// Launch the NPET data processor which needs to be installed separately.
/// The data processor is a Python package that processes the raw measurement data and generates plots and reports.
int launchDataProcessor();

void printAppIntro();

///
/// Lists available COM ports and prompts the user to select one.
/// If autoselect is true and only one COM port is available, it will be selected automatically.
/// Ports listed in EXCLUDED_PORTS are dropped from the selection before it is shown.
/// WARNING: This function does not check if the selected COM port is valid.
/// @param AUTOSELECT If true, automatically select the COM port if only one is available.
/// @param EXCLUDED_PORTS COM port numbers to exclude from the selection (e.g. {5, 8}).
/// @throws runtime_error if no COM ports are found.
/// @returns Selected COM port number (e.g. 8 for COM8).
int selectComPortCli(bool AUTOSELECT, const std::vector<int> &EXCLUDED_PORTS = {});

///
/// Open communication for the referenced NPET on the provided NPET.
/// Errors are handled internally and False is returned if any errors are encountered.
/// @param npet The NPETComm reference
/// @param COM_PORT COM port number
/// @param ERROR_MSG Error message in case of an error
/// @return True if communication was successfully opened, False otherwise
bool openCommSafe(NPETComm &npet, int COM_PORT, std::string_view ERROR_MSG);

///
/// Reset all the NPET settings
/// @param npet The NPETComm reference
/// @param DESIGNATION Optional designation for the NPET, used in error messages. If empty, a default message will be used.
void resetNPETSafe(NPETComm &npet, std::string_view DESIGNATION = "");

///
/// Data validation allows either a positive integer number or (optionally) -1 for infinity.
/// @param num_to_validate Number to check as string
/// @param ALLOW_NEGATIVE_ONE Whether to allow -1 as a valid input
/// @return The validated number in int format, or -2 if the input is invalid
int numValidation(const std::string &num_to_validate, bool ALLOW_NEGATIVE_ONE = true);

///
/// Prompt the user to select a measurement channel
/// @return Selected channel
std::optional<Channel> promptChannel(int DEFAULT_CHANNEL, std::string_view PROMPT_MSG = "channel to read from");

///
/// Prompt the user to select a baud rate.
/// @param CURRENT_BAUD_RATE The current baud rate of the NPET communication.
/// @return Selected baud rate
int promptBaudRate(INT CURRENT_BAUD_RATE);

///
/// Change communication baud rate for the referenced NPET.
/// @param npet The NPETComm reference
/// @param NEW_BAUD_RATE The new baud rate to set for the NPET communication
void setBaudRateSafe(NPETComm &npet, int NEW_BAUD_RATE);

///
/// Prompt the user to define the NPET internal FW version.
/// @param CURRENT_FW_VERSION The current firmware version of the NPET.
/// @return Selected FW version
FWVersion promptFWVersion(FWVersion CURRENT_FW_VERSION);

///
/// Prompt the user to define the integer part of the time correction constant
/// @param SEL Time correction constant int part selection logic
/// @return Time correction constant in seconds
int promptTimeConstSeconds(ConstIntSelectionLogic SEL);

///
/// Prompt the user to define a new time correction constat in raw format.
/// @param OLD_CONST The existing time correction constant
/// @return The new time correction constant
Measurement promptRawTimeConstant(const Measurement &OLD_CONST);

#endif //NPET_COMM_FW_HELPER_FUNC_H
