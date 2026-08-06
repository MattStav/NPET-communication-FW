#include "test_framework_workflow.h"

#include <gtest/gtest.h>

std::unique_ptr<VirtualMachine> FrameworkWorkflowFixture::vm;
std::unique_ptr<std::jthread> FrameworkWorkflowFixture::vm_thread;
std::unique_ptr<NPETComm> FrameworkWorkflowFixture::client;

///
/// Opens both ends of the com0com virtual null-modem pair and starts the VM's deviceLoop() on a
/// background thread, once for the whole suite.
void FrameworkWorkflowFixture::SetUpTestSuite() {
    vm = std::make_unique<VirtualMachine>(100);
    vm->openCommunication(VM_COM_PORT, BAUD_RATE);
    vm_thread = std::make_unique<std::jthread>([] { vm->deviceLoop(); });

    client = std::make_unique<NPETComm>();
    client->openCommunication(CLIENT_COM_PORT, BAUD_RATE);
}

///
/// Cancelling the VM's port unblocks its pending read inside deviceLoop() with an
/// OperationCancelledError, which deviceLoop() treats as a clean shutdown signal (see
/// virtual_machine.cpp); resetting vm_thread_ then joins once that return happens.
void FrameworkWorkflowFixture::TearDownTestSuite() {
    if (client) {
        client->closeCommunication();
    }
    if (vm && vm->isOpen()) {
        vm->getPort().cancel();
    }
    vm_thread.reset();
    vm.reset();
    client.reset();
}

// Proves the setup itself: two virtual ports paired via com0com, a VirtualMachine listening on
// one end, and a plain NPETComm client on the other, actually exchange bytes end to end. Further
// workflow tests build on this same live connection.
TEST_F(FrameworkWorkflowFixture, ClientIsResponsiveToVirtualMachine) {
    ASSERT_TRUE(client->isOpen());
    ASSERT_TRUE(vm->isOpen());
    EXPECT_TRUE(client->isResponsive());
}
