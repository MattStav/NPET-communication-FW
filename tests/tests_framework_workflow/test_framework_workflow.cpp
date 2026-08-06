#include "test_framework_workflow.h"

#include <gtest/gtest.h>
#include <string>

// NPETComm tests below reuse the single live connection brought up by SetUpTestSuite (see
// FrameworkWorkflowFixture) and are therefore order-dependent: GoogleTest runs TEST_F cases
// within a fixture in source-declaration order, and each test below relies on the device state
// left behind by the ones above it (e.g. the exported time constant). Keep that in mind before
// reordering or interleaving new tests.

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

TEST_F(FrameworkWorkflowFixture, SetBaudRate) {
    ASSERT_TRUE(client->setBaudRate(230400));
    EXPECT_TRUE(client->isResponsive());
    boost::asio::serial_port_base::baud_rate current_baud{};
    client->getPort().get_option(current_baud);
    EXPECT_EQ(current_baud.value(), 230400);
}

// The VM answers "?" with "Firmware none - offline", which detectFWVer() recognizes as the
// Virtual NPET firmware.
TEST_F(FrameworkWorkflowFixture, DetectFWVerSetsVirtualVersion) {
    client->detectFWVer();
    EXPECT_EQ(client->fw_version, FWVersion(FWVersion::VIRTUAL));
}

TEST_F(FrameworkWorkflowFixture, SetFrequencySucceeds) {
    EXPECT_TRUE(client->setFrequency(500));
}

TEST_F(FrameworkWorkflowFixture, GeneratePulsesSucceedsForFiniteCount) {
    EXPECT_TRUE(client->generatePulses(5));
}

TEST_F(FrameworkWorkflowFixture, GeneratePulsesSucceedsForInfiniteCount) {
    EXPECT_TRUE(client->generatePulses(-1));
}

TEST_F(FrameworkWorkflowFixture, GeneratePulsesSucceedsForStop) {
    EXPECT_TRUE(client->generatePulses(0));
}

TEST_F(FrameworkWorkflowFixture, SetMeasuredDataFormatSucceedsForBinary) {
    EXPECT_TRUE(client->setMeasuredDataFormat(0));
}

TEST_F(FrameworkWorkflowFixture, SetMeasuredDataFormatSucceedsForAscii) {
    EXPECT_TRUE(client->setMeasuredDataFormat(1));
}

TEST_F(FrameworkWorkflowFixture, ExportedRawTimeConstantRoundTripsThroughImport) {
    const std::string RAW_CONST = "5 0.500000000000000";
    ASSERT_TRUE(client->exportTimeConstantRaw(RAW_CONST));
    EXPECT_EQ(client->importTimeConstantRaw(), RAW_CONST);
}

// TODO: More test cases
TEST_F(FrameworkWorkflowFixture, ExportTimeConstantSucceeds) {
    const Measurement CONSTANT{.meas_num = -1, .intp = 42, .fracp = 0.25};
    EXPECT_TRUE(client->exportTimeConstant(CONSTANT));
    EXPECT_EQ(client->importTimeConstantRaw(), CONSTANT.toString());
}

// clearTimeConstant writes 28 spaces; the VM echoes them back verbatim on import, which
// importTimeConstant() recognizes (via the space at index 4 of the raw response) as "no constant
// set" and reports as an empty Measurement.
TEST_F(FrameworkWorkflowFixture, ClearTimeConstantResultsInEmptyImportedConstant) {
    const std::string RAW_CONST = "5 0.500000000000000";
    ASSERT_TRUE(client->exportTimeConstantRaw(RAW_CONST));
    ASSERT_TRUE(client->clearTimeConstant());
    EXPECT_TRUE(client->importTimeConstant().isEmpty());
    EXPECT_EQ(client->importTimeConstantRaw(), "");
}


///
///
///
///
///
/// THIS TEST MUST BE THE LAST
TEST_F(FrameworkWorkflowFixture, IsResponsiveAtTheEnd) {
    EXPECT_TRUE(client->isResponsive());
}
