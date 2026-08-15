#include <gtest/gtest.h>
#include <array>
#include <chrono>
#include <thread>

#include "serial.h"
#include "test_workflow_fixture.h"

///
/// Polls SERIAL until PRED is true or TIMEOUT elapses, whichever comes first. Serial::pollUntil()
/// alone has no timeout concept, so a predicate that's never satisfied (e.g. testing that
/// something correctly did NOT happen) would otherwise hang the test forever.
template<typename Predicate>
static void pollWithDeadline(Serial &serial, Predicate pred, const std::chrono::milliseconds TIMEOUT) {
    const auto DEADLINE = std::chrono::steady_clock::now() + TIMEOUT;
    serial.pollUntil([&] { return pred() || std::chrono::steady_clock::now() >= DEADLINE; },
                     true, std::chrono::milliseconds(1));
}

///
/// Two Serial objects talking directly to each other over the virtual null-modem pair,
/// with no VirtualMachine or NPETComm layered on top - just the raw Serial API (open/write/read/
/// cancel/listen) exercised end to end on both sides.
class SerialCommunicationWorkflow : public ::testing::Test {
protected:
    Serial side_a;
    Serial side_b;

    void SetUp() override {
        try {
            side_a.openCommunication(VM_COM_PORT, BAUD_RATE);
            side_b.openCommunication(CLIENT_COM_PORT, BAUD_RATE);
        } catch (const std::exception &e) {
            GTEST_SKIP() << "Could not open COM" << VM_COM_PORT << "/COM" << CLIENT_COM_PORT << ": "
                    << e.what() << ". This test requires a com0com virtual null-modem pair on COM"
                    << VM_COM_PORT << "/COM" << CLIENT_COM_PORT << " - skipping.";
        }
    }

    void TearDown() override {
        side_a.closeCommunication();
        side_b.closeCommunication();
    }
};

// A line written on one side must be readable, verbatim, on the other side. readWithTimeout
// strips the trailing \r\n that writeToSerial appends.
TEST_F(SerialCommunicationWorkflow, WriteToSerialIsReadableAsLine) {
    side_a.writeToSerial("hello");
    const std::vector<char> RECEIVED = side_b.readWithTimeout(ReadMode::UNTIL_NEWLINE, std::chrono::milliseconds(500));
    EXPECT_EQ(std::string(RECEIVED.begin(), RECEIVED.end()), "hello\r\n");
}

TEST_F(SerialCommunicationWorkflow, ReadFromSerialSeesRawBytesAsWritten) {
    side_a.writeToSerial("ping");
    std::string response;
    const auto DEADLINE = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (response.size() < 6 && std::chrono::steady_clock::now() < DEADLINE) {
        response += side_b.readFromSerial(64);
    }
    EXPECT_EQ(response, "ping\r\n");
}

// Raw binary payloads (e.g. measurement packets) must round-trip byte-for-byte without the \r\n
// line-termination writeToSerial adds - exactly what encoded measurement frames rely on to avoid
// desyncing the receiver.
TEST_F(SerialCommunicationWorkflow, WriteRawToSerialRoundTripsExactBytes) {
    constexpr std::array<std::uint8_t, 4> SENT{0x01, 0x0B, 0xFF, 0x00};
    side_a.writeRawToSerial(SENT);

    std::array<std::uint8_t, 4> received{};
    boost::system::error_code ec;
    bool completed = false;
    side_b.readExactAsync(received, ec, completed);
    pollWithDeadline(side_b, [&] { return completed; }, std::chrono::milliseconds(500));

    ASSERT_TRUE(completed) << "readExactAsync did not complete within the deadline";
    ASSERT_FALSE(ec) << ec.message();
    EXPECT_EQ(received, SENT);
}

// listenForCommand must flag MATCHED when the received line starts with the expected byte - the
// exact mechanism VirtualMachine/NPETComm use to detect an incoming stop command mid-stream.
TEST_F(SerialCommunicationWorkflow, ListenForCommandMatchesExpectedFirstByte) {
    bool matched = false;
    side_b.listenForCommand('p', matched);
    side_a.writeToSerial("pong");
    pollWithDeadline(side_b, [&] { return matched; }, std::chrono::milliseconds(500));
    EXPECT_TRUE(matched);
}

// listenForCommand must leave MATCHED untouched when the received line starts with a different
// byte than expected.
TEST_F(SerialCommunicationWorkflow, ListenForCommandIgnoresUnexpectedFirstByte) {
    bool matched = false;
    side_b.listenForCommand('z', matched);
    side_a.writeToSerial("pong");
    // Nothing will ever set matched here; just give the (non-matching) line time to be read
    // and processed before asserting it stayed false.
    pollWithDeadline(side_b, [&] { return false; }, std::chrono::milliseconds(300));
    EXPECT_FALSE(matched);
}

// cancelPendingOperation must actually unblock a read that has nothing to receive, turning it
// into an OperationCancelledError instead of leaving it to wait out the full timeout.
TEST_F(SerialCommunicationWorkflow, CancelPendingOperationInterruptsBlockingRead) {
    std::jthread const canceller([this] {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        side_b.cancelPendingOperation(false);
    });
    // side_a never writes anything, so without the canceller thread this would block for the
    // full 5-second timeout instead of throwing almost immediately.
    EXPECT_THROW(
        side_b.readWithTimeout(ReadMode::UNTIL_NEWLINE, std::chrono::seconds(5)),
        OperationCancelledError);
}
