#ifndef NPET_COMM_FW_VM_MAIN_H
#define NPET_COMM_FW_VM_MAIN_H

#include "virtual_machine.h"

///
/// Main function for the mock NPET device virtual machine.
/// Launches a mock NPET device that communicates over a specified COM port.
/// Requires virtual com ports to work!!!
/// @param CONFIG Virtual machine configurations
int launchVm(VmConfig CONFIG);


#endif //NPET_COMM_FW_VM_MAIN_H
