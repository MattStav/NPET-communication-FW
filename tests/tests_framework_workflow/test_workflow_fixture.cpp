#include "test_workflow_fixture.h"

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
/// Checks the connection is still responsive before tearing it down, so a test that silently
/// broke it fails loudly here rather than just letting the next group open a fresh connection
/// over it. Cancelling the VM's port then unblocks its pending read inside deviceLoop() with an
/// OperationCancelledError, which deviceLoop() treats as a clean shutdown signal (see
/// virtual_machine.cpp); resetting vm_thread then joins once that return happens.
void FrameworkWorkflowFixture::TearDownTestSuite() {
    if (client && client->isOpen() && vm && vm->isOpen()) {
        EXPECT_TRUE(client->isResponsive()) << "Connection was no longer responsive at teardown";
    }
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
