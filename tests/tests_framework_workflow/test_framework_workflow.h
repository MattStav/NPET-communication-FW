#ifndef TEST_FRAMEWORK_WORKFLOW_H
#define TEST_FRAMEWORK_WORKFLOW_H

#include <gtest/gtest.h>
#include <memory>
#include <thread>

#include "NPET_comm.h"
#include "virtual_machine.h"

// The two ends of a com0com virtual null-modem pair, provisioned ahead of the test run
inline constexpr int VM_COM_PORT = 8;
inline constexpr int CLIENT_COM_PORT = 9;
inline constexpr int BAUD_RATE = 115200;

// Brings up a VirtualMachine on one end of the paired virtual ports and an NPETComm client on
// the other, once for the whole test suite, so later workflow tests can reuse this single live
// connection instead of re-pairing/re-opening ports per test.
class FrameworkWorkflowFixture : public ::testing::Test {
protected:
    static std::unique_ptr<VirtualMachine> vm;
    static std::unique_ptr<std::jthread> vm_thread;
    static std::unique_ptr<NPETComm> client;

    static void SetUpTestSuite();

    static void TearDownTestSuite();
};

#endif //TEST_FRAMEWORK_WORKFLOW_H
