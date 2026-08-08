#ifndef TEST_VIRTUAL_MACHINE_H
#define TEST_VIRTUAL_MACHINE_H

#include <gtest/gtest.h>
#include "virtual_machine.h"

// Base fixture. A moderate, valid channel-1 frequency is used so the fixture never trips
// the >2500Hz validation that launchVm performs separately.
class VirtualMachineFixture : public ::testing::Test {
protected:
    VirtualMachine vm{VmConfig{.com_port = 1, .ch1_frequency = 100, .corrupt_every = 0}};
};

#endif //TEST_VIRTUAL_MACHINE_H
