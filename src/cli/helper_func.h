#ifndef NPET_COMM_FW_HELPER_FUNC_H
#define NPET_COMM_FW_HELPER_FUNC_H
#include <vector>

#include "NPET_comm.h"
// Forward declaration
class NPETCommCLI;

constexpr int DEFAULT_BAUD_RATE = 115200;
constexpr int INVALID_NUM_SENTINEL = -2;
constexpr std::string_view INVALID_COM_PORT = "Invalid COM port number";
constexpr std::string_view FAILED_OPEN_COM_PORT = "Failed to open the COM port: {}";
constexpr std::string_view FAILED_OPEN_COM_PORT_MAX_ATTEMPT = "Failed to open NPET communication after {} attempts";
constexpr std::string_view INVALID_NUM = "Invalid input. Number out of allowed range";
constexpr std::string_view PULSE_GEN_STOP_FAIL = "Failed to stop pulse generation";
constexpr std::string_view FREQ_RESET_FAIL = "Failed to reset the pulse generation frequency";
constexpr std::string_view BAUD_RATE_RESET_FAIL = "Failed to reset baud rate to default";
constexpr std::string_view DATA_FORMAT_RESET_FAIL = "Failed to reset the data format";
constexpr std::string_view TIME_CONST_FAILED_TO_CLEAR = "Failed to clear the time correction constant saved in NPET";

int printManual();

int printLicenseInformation();

int resetNpetStandalone();

int launchDataProcessor();

int selectComPortCli(bool AUTOSELECT, const std::vector<int> &EXCLUDED_PORTS = {});

bool openCommSafe(NPETComm &npet, int COM_PORT, std::string_view ERROR_MSG);

int numValidation(const std::string &num_to_validate, bool ALLOW_NEGATIVE_ONE = true);

std::optional<Channel> promptChannel(int DEFAULT_CHANNEL, std::string_view PROMPT_MSG = "channel to read from");

int promptBaudRate(INT CURRENT_BAUD_RATE);

void setBaudRateSafe(NPETComm &npet, int NEW_BAUD_RATE);

void printAppIntro();

#endif //NPET_COMM_FW_HELPER_FUNC_H
