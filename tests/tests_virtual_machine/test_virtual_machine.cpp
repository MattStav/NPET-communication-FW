#include "test_virtual_machine.h"

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>
#include <sstream>
#include <thread>

#include "serial.h"

using ::testing::MatchesRegex;

// Exposes VirtualMachine's protected getRunTime so it can be tested directly,
// without going through deviceLoop()'s logging.
class TestableVirtualMachine final : public VirtualMachine {
public:
    using VirtualMachine::VirtualMachine;
    using VirtualMachine::getRunTime;
};

TEST_F(VirtualMachineFixture, ConstructedIsNotOpen) {
    EXPECT_FALSE(vm.ser.isOpen());
}

TEST_F(VirtualMachineFixture, CloseCommunicationOnUnopenedVmDoesNotThrow) {
    EXPECT_NO_THROW(vm.ser.closeCommunication());
    EXPECT_FALSE(vm.ser.isOpen());
}

// getRunTime() must always report elapsed time as zero-padded hh:mm:ss, the format
// deviceLoop() logs as the VM's runtime status line.
// gtest's simple regex engine has no {n} repetition or [...] character classes (see
// test_meas_func.cpp's make_pattern for the same workaround), so digits are spelled out.
TEST(VirtualMachineGetRunTime, FormatIsHhMmSs) {
    const TestableVirtualMachine VM{VmConfig{}};
    EXPECT_THAT(VM.getRunTime(), MatchesRegex(R"(\d\d:\d\d:\d\d)"));
}

// Immediately after construction, almost no time has elapsed, so the runtime must still
// read 00 hours and 00 minutes. Parsed and compared numerically (rather than matched with a
// regex range, which gtest's simple regex engine can't express) with a few seconds of slack
// on the seconds field to tolerate slow test machines without making the test flaky.
TEST(VirtualMachineGetRunTime, StartsNearZero) {
    const TestableVirtualMachine VM{VmConfig{}};
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    char colon1 = 0;
    char colon2 = 0;
    std::istringstream(VM.getRunTime()) >> hours >> colon1 >> minutes >> colon2 >> seconds;
    EXPECT_EQ(hours, 0);
    EXPECT_EQ(minutes, 0);
    EXPECT_LT(seconds, 5);
}

// getRunTime() must reflect actual elapsed wall-clock time since construction, not a
// value fixed once and never updated.
// getRunTime() truncates to whole seconds, so a single fixed-length sleep can undershoot
// the next second boundary (e.g. due to sleep_for waking up slightly early) and make this
// test flaky. Instead, poll until the value actually changes, bounded by a generous timeout.
TEST(VirtualMachineGetRunTime, IncreasesAfterDelay) {
    const TestableVirtualMachine VM{VmConfig{}};
    const std::string BEFORE = VM.getRunTime();
    const auto DEADLINE = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    std::string after = BEFORE;
    while (after == BEFORE && std::chrono::steady_clock::now() < DEADLINE) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        after = VM.getRunTime();
    }
    EXPECT_NE(BEFORE, after);
}
