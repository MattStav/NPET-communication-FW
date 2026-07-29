#ifndef NPET_COMM_FW_VM_MAIN_H
#define NPET_COMM_FW_VM_MAIN_H


struct VmConfig {
    int com_port;
    int ch1_frequency;
};

int launchVm(VmConfig CONFIG);


#endif //NPET_COMM_FW_VM_MAIN_H
