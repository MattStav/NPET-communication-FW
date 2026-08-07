#ifndef NPET_COMM_FW_HELPER_FUNC_H
#define NPET_COMM_FW_HELPER_FUNC_H
#include "NPET_comm_CLI.h"

constexpr int DEFAULT_BAUD_RATE = 115200;

int printManual();

int printLicenseInformation();

int resetNpetStandalone();

int launchDataProcessor();

void settingsMenu(NPETCommCLI &npet_comm);

void printAppIntro();

#endif //NPET_COMM_FW_HELPER_FUNC_H
