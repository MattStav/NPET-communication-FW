#include "test_virtual_machine.h"

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>
#include <sstream>
#include <thread>
#include <type_traits>

#include "serial.h"

using ::testing::MatchesRegex;

// Exposes VirtualMachine's protected getRunTime so it can be tested directly,
// without going through deviceLoop()'s logging.
class TestableVirtualMachine final : public VirtualMachine {
public:
    using VirtualMachine::VirtualMachine;
    using VirtualMachine::getRunTime;
};

// VirtualMachine only adds device-simulation behavior on top of SerialMachine; every
// public/protected communication primitive it relies on (open/close/isOpen/getIO/getPort)
// must come from the base class.
TEST(VirtualMachineType, IsASerialMachine) {
    EXPECT_TRUE((std::is_base_of_v<Serial, VirtualMachine>));
}

TEST_F(VirtualMachineFixture, ConstructedIsNotOpen) {
    EXPECT_FALSE(vm.isOpen());
}

TEST_F(VirtualMachineFixture, GetIOReturnsSameInstanceOnRepeatedCalls) {
    EXPECT_EQ(&vm.getIO(), &vm.getIO());
}

TEST_F(VirtualMachineFixture, GetPortReturnsSameInstanceOnRepeatedCalls) {
    EXPECT_EQ(&vm.getPort(), &vm.getPort());
}

TEST_F(VirtualMachineFixture, CloseCommunicationOnUnopenedVmDoesNotThrow) {
    EXPECT_NO_THROW(vm.closeCommunication());
    EXPECT_FALSE(vm.isOpen());
}

// Two independently constructed VMs must not secretly share the io_context/port owned by
// the SerialMachine base, otherwise closing/opening one would affect the other.
TEST(VirtualMachineState, DistinctInstancesOwnDistinctIO) {
    VirtualMachine vm_a{VmConfig{}};
    VirtualMachine vm_b{VmConfig{}};
    EXPECT_NE(&vm_a.getIO(), &vm_b.getIO());
    EXPECT_NE(&vm_a.getPort(), &vm_b.getPort());
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
TEST(VirtualMachineGetRunTime, IncreasesAfterDelay) {
    const TestableVirtualMachine VM{VmConfig{}};
    const std::string BEFORE = VM.getRunTime();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const std::string AFTER = VM.getRunTime();
    EXPECT_NE(BEFORE, AFTER);
}
