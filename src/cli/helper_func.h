#ifndef NPET_COMM_FW_HELPER_FUNC_H
#define NPET_COMM_FW_HELPER_FUNC_H
#include <vector>

#include "NPET_comm.h"
// Forward declaration
class NPETCommCLI;

constexpr int DEFAULT_BAUD_RATE = 115200;
constexpr std::string_view INVALID_COM_PORT = "Invalid COM port number";
constexpr std::string_view FAILED_OPEN_COM_PORT = "Failed to open the COM port: {}";
constexpr std::string_view FAILED_OPEN_COM_PORT_MAX_ATTEMPT = "Failed to open NPET communication after {} attempts";

int printManual();

int printLicenseInformation();

int resetNpetStandalone();

int launchDataProcessor();

int selectComPortCli(bool AUTOSELECT, const std::vector<int> &EXCLUDED_PORTS = {});

bool openCommSafe(NPETComm &npet, int COM_PORT, std::string_view ERROR_MSG);

void settingsMenu(NPETCommCLI &npet_comm);

void printAppIntro();

#endif //NPET_COMM_FW_HELPER_FUNC_H
