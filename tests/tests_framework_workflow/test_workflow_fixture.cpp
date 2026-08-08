#include "test_workflow_fixture.h"

std::unique_ptr<VirtualMachine> FrameworkWorkflowFixture::vm;
std::unique_ptr<std::jthread> FrameworkWorkflowFixture::vm_thread;
std::unique_ptr<NPETComm> FrameworkWorkflowFixture::client;

///
/// Opens both ends of the com0com virtual null-modem pair and starts the VM's deviceLoop() on a
/// background thread, once for the whole suite. Ports are opened before the device-loop thread is
/// started so a missing/unpaired port fails here with nothing left to tear down.
void FrameworkWorkflowFixture::SetUpTestSuite() {
    vm = std::make_unique<VirtualMachine>(VmConfig{.com_port = VM_COM_PORT, .ch1_frequency = 100, .corrupt_every = 0});
    client = std::make_unique<NPETComm>();
    try {
        vm->openCommunication(VM_COM_PORT, BAUD_RATE);
        client->openCommunication(CLIENT_COM_PORT, BAUD_RATE);
    } catch (const std::exception &e) {
        vm.reset();
        client.reset();
        GTEST_SKIP() << "Could not open COM" << VM_COM_PORT << "/COM" << CLIENT_COM_PORT << ": "
                     << e.what() << ". This suite requires a com0com virtual null-modem pair on "
                        "these ports - skipping.";
    }
    vm_thread = std::make_unique<std::jthread>([] { vm->deviceLoop(); });
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
