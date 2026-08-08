#include <gtest/gtest.h>
#include <chrono>

#include "serial_machine.h"
#include "test_workflow_fixture.h"

// Exposes SerialMachine's protected read_with_timeout so the timeout behavior can be
// exercised directly, without going through a higher-level command/response exchange.
class TestableSerialMachine final : public SerialMachine {
public:
    using SerialMachine::ReadMode;
    using SerialMachine::readWithTimeout;
};

// Opens one end of the com0com virtual null-modem pair and leaves the other end untouched, so a
// read genuinely has nothing to receive and the timeout path (rather than a real response) is
// what fires. Uses the same pair/port constants as FrameworkWorkflowFixture
TEST(SerialMachineTimeout, ReadWithTimeoutThrowsCommTimeoutErrorWhenNoDataArrives) {
    TestableSerialMachine machine;
    try {
        machine.openCommunication(VM_COM_PORT, BAUD_RATE);
    } catch (const std::exception &e) {
        GTEST_SKIP() << "Could not open COM" << VM_COM_PORT << ": " << e.what()
                << ". This test requires a com0com virtual null-modem pair on COM"
                << VM_COM_PORT << "/COM" << CLIENT_COM_PORT << " - skipping.";
    }

    EXPECT_THROW(
        machine.readWithTimeout(TestableSerialMachine::ReadMode::UNTIL_NEWLINE, std::chrono::milliseconds(50)),
        CommTimeoutError);

    machine.closeCommunication();
}
