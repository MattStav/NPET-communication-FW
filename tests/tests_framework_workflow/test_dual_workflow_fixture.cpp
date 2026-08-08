#include "test_dual_workflow_fixture.h"

std::unique_ptr<VirtualMachine> DualFrameworkWorkflowFixture::start_vm;
std::unique_ptr<VirtualMachine> DualFrameworkWorkflowFixture::stop_vm;
std::unique_ptr<std::jthread> DualFrameworkWorkflowFixture::start_vm_thread;
std::unique_ptr<std::jthread> DualFrameworkWorkflowFixture::stop_vm_thread;

///
/// Opens both ends of both com0com virtual null-modem pairs and starts each VM's deviceLoop() on
/// its own background thread, once for the whole suite. Both ports are opened before either
/// device-loop thread starts so a missing/unpaired port fails here with nothing left to tear down.
void DualFrameworkWorkflowFixture::SetUpTestSuite() {
    start_vm = std::make_unique<VirtualMachine>(
        VmConfig{.com_port = START_VM_COM_PORT, .ch1_frequency = 100, .corrupt_every = 0});
    stop_vm = std::make_unique<VirtualMachine>(
        VmConfig{.com_port = STOP_VM_COM_PORT, .ch1_frequency = 100, .corrupt_every = 0});
    try {
        start_vm->openCommunication(START_VM_COM_PORT, BAUD_RATE);
        stop_vm->openCommunication(STOP_VM_COM_PORT, BAUD_RATE);
    } catch (const std::exception &e) {
        start_vm.reset();
        stop_vm.reset();
        GTEST_SKIP() << "Could not open COM" << START_VM_COM_PORT << "/COM" << START_CLIENT_COM_PORT
                     << " and/or COM" << STOP_VM_COM_PORT << "/COM" << STOP_CLIENT_COM_PORT << ": "
                     << e.what() << ". This suite requires two com0com virtual null-modem pairs, "
                        "on these ports - skipping.";
    }
    start_vm_thread = std::make_unique<std::jthread>([] { start_vm->deviceLoop(); });
    stop_vm_thread = std::make_unique<std::jthread>([] { stop_vm->deviceLoop(); });
}

///
/// Cancels both VMs' pending reads to unblock deviceLoop() with an OperationCancelledError, which
/// it treats as a clean shutdown signal (see virtual_machine.cpp); resetting the *_vm_thread
/// members then joins once that return happens.
void DualFrameworkWorkflowFixture::TearDownTestSuite() {
    if (start_vm && start_vm->isOpen()) {
        start_vm->getPort().cancel();
    }
    if (stop_vm && stop_vm->isOpen()) {
        stop_vm->getPort().cancel();
    }
    start_vm_thread.reset();
    stop_vm_thread.reset();
    start_vm.reset();
    stop_vm.reset();
}

///
/// Opens NPETDual's start_/stop_ legs onto the two VMs' client-side ports for this one test
void DualFrameworkWorkflowFixture::SetUp() {
    try {
        one_.openCommunication(START_CLIENT_COM_PORT, BAUD_RATE);
        two_.openCommunication(STOP_CLIENT_COM_PORT, BAUD_RATE);
        one_.detectFWVer();
        two_.detectFWVer();
    } catch (const std::exception &e) {
        GTEST_SKIP() << "Could not open COM" << START_CLIENT_COM_PORT << "/COM" << STOP_CLIENT_COM_PORT
                     << " for this test: " << e.what();
    }
}

///
/// Checks both connections are still responsive before this test's start_/stop_ get destroyed
/// (and, with them, closed), so a test that silently broke one shows up as its own failure
/// instead of quietly poisoning whichever later test reopens the same port.
void DualFrameworkWorkflowFixture::TearDown() {
    if (one_.isOpen() && two_.isOpen()) {
        EXPECT_TRUE(one_.isResponsive()) << "start_ connection was no longer responsive at teardown";
        EXPECT_TRUE(two_.isResponsive()) << "stop_ connection was no longer responsive at teardown";
    }
}
