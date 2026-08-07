#ifndef NPET_COMM_FW_HELPER_FUNC_H
#define NPET_COMM_FW_HELPER_FUNC_H
#include <vector>

#include "NPET_comm_CLI.h"

constexpr int DEFAULT_BAUD_RATE = 115200;

int printManual();

int printLicenseInformation();

int resetNpetStandalone();

int launchDataProcessor();

int selectComPortCli(bool AUTOSELECT, const std::vector<int> &EXCLUDED_PORTS = {});

void settingsMenu(NPETCommCLI &npet_comm);

void printAppIntro();

#endif //NPET_COMM_FW_HELPER_FUNC_H
