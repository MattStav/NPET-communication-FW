#include "test_virtual_machine.h"

#include <gtest/gtest.h>
#include <type_traits>

#include "serial_machine.h"


// VirtualMachine only adds device-simulation behavior on top of SerialMachine; every
// public/protected communication primitive it relies on (open/close/isOpen/getIO/getPort)
// must come from the base class.
TEST(VirtualMachineType, IsASerialMachine) {
    EXPECT_TRUE((std::is_base_of_v<SerialMachine, VirtualMachine>));
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
