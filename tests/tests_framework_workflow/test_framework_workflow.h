#ifndef TEST_FRAMEWORK_WORKFLOW_H
#define TEST_FRAMEWORK_WORKFLOW_H

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <quadmath.h>

#include "NPET_comm.h"
#include "virtual_machine.h"

// The two ends of a com0com virtual null-modem pair, provisioned ahead of the test run
inline constexpr int VM_COM_PORT = 8;
inline constexpr int CLIENT_COM_PORT = 9;
inline constexpr int BAUD_RATE = 115200;

// Brings up a VirtualMachine on one end of the paired virtual ports and an NPETComm client on
// the other, once per test suite, so tests within a suite can reuse this single live connection
// instead of re-pairing/re-opening ports per test. Derived (e.g. TEST_P) fixtures below get their
// own instance of this setup/teardown cycle, since GoogleTest scopes SetUpTestSuite()/
// TearDownTestSuite() per fixture class, not per translation unit; TearDownTestSuite() checks
// isResponsive() right before closing, so every group verifies its connection stayed healthy.
class FrameworkWorkflowFixture : public ::testing::Test {
protected:
    static std::unique_ptr<VirtualMachine> vm;
    static std::unique_ptr<std::jthread> vm_thread;
    static std::unique_ptr<NPETComm> client;

    static void SetUpTestSuite();

    static void TearDownTestSuite();
};

// "name" must contain only letters, digits, and underscores - no spaces, dots, or signs.
struct FrequencyParams {
    std::string name;
    int frequency;
};

struct PulseCountParams {
    std::string name;
    int num_of_pulses;
};

struct TimeConstantParams {
    std::string name;
    int intp;
    __float128 fracp;
};

#endif //TEST_FRAMEWORK_WORKFLOW_H
