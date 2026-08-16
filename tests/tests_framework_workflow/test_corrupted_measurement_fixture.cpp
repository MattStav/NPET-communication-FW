#include "test_corrupted_measurement_fixture.h"

#include "test_workflow_fixture.h"

std::unique_ptr<VirtualMachine> CorruptedMeasurementWorkflowFixture::vm;
std::unique_ptr<std::jthread> CorruptedMeasurementWorkflowFixture::vm_thread;
std::unique_ptr<NPETComm> CorruptedMeasurementWorkflowFixture::client;

///
/// Opens both ends of the virtual null-modem pair and starts the VM's deviceLoop() on a
/// background thread, once for the whole suite. Unlike FrameworkWorkflowFixture's VM, this one is
/// configured with corrupt_every = CORRUPT_EVERY, so it deliberately flips the checksum byte on
/// every CORRUPT_EVERY-th measurement it sends. Ports are opened before the device-loop thread is
/// started so a missing/unpaired port fails here with nothing left to tear down.
void CorruptedMeasurementWorkflowFixture::SetUpTestSuite() {
    vm = std::make_unique<VirtualMachine>(
        VmConfig{.com_port = VM_COM_PORT, .ch1_frequency = 100, .corrupt_every = CORRUPT_EVERY});
    client = std::make_unique<NPETComm>();
    try {
        vm->ser.openCommunication(VM_COM_PORT, BAUD_RATE);
        client->ser.openCommunication(CLIENT_COM_PORT, BAUD_RATE);
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
void CorruptedMeasurementWorkflowFixture::TearDownTestSuite() {
    if (client && client->ser.isOpen() && vm && vm->ser.isOpen()) {
        EXPECT_TRUE(client->isResponsive()) << "Connection was no longer responsive at teardown";
    }
    if (client) {
        client->ser.closeCommunication();
    }
    if (vm && vm->ser.isOpen()) {
        vm->ser.cancelPendingOperation(false);
    }
    vm_thread.reset();
    vm.reset();
    client.reset();
}

///
/// All measurement reads decode using client's fw_version's multiplier, which must match the
/// multiplier the VM encoded with (VIRTUAL) or decoded values come out meaningless.
void CorruptedMeasurementWorkflowFixture::SetUp() {
    if (client) {
        client->detectFWVer();
        EXPECT_EQ(client->getFWVer(), FWVersion(FWVersion::VIRTUAL));
    }
}
