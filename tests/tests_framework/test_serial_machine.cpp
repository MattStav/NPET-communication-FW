#include "serial_machine.h"

#include <gtest/gtest.h>
#include <type_traits>


TEST(SerialMachineState, DefaultConstructedIsNotOpen) {
    const Serial MACHINE;
    EXPECT_FALSE(MACHINE.isOpen());
}

TEST(SerialMachineState, CloseCommunicationOnUnopenedMachineDoesNotThrow) {
    Serial machine;
    EXPECT_NO_THROW(machine.closeCommunication());
    EXPECT_FALSE(machine.isOpen());
}

TEST(SerialMachineState, CloseCommunicationIsIdempotent) {
    Serial machine;
    machine.closeCommunication();
    EXPECT_NO_THROW(machine.closeCommunication());
    EXPECT_NO_THROW(machine.closeCommunication());
    EXPECT_FALSE(machine.isOpen());
}

TEST(CommTimeoutErrorTest, PreservesMessage) {
    const CommTimeoutError ERR("timed out after 2000ms");
    EXPECT_STREQ(ERR.what(), "timed out after 2000ms");
}

TEST(CommTimeoutErrorTest, IsCatchableAsRuntimeError) {
    try {
        throw CommTimeoutError("boom");
    } catch (const std::runtime_error &e) {
        EXPECT_STREQ(e.what(), "boom");
        SUCCEED();
        return;
    }
    FAIL() << "CommTimeoutError was not caught as std::runtime_error";
}

TEST(OperationCancelledErrorTest, PreservesMessage) {
    const OperationCancelledError ERR("read cancelled");
    EXPECT_STREQ(ERR.what(), "read cancelled");
}

TEST(OperationCancelledErrorTest, IsCatchableAsRuntimeError) {
    try {
        throw OperationCancelledError("boom");
    } catch (const std::runtime_error &e) {
        EXPECT_STREQ(e.what(), "boom");
        SUCCEED();
        return;
    }
    FAIL() << "OperationCancelledError was not caught as std::runtime_error";
}

// The two error types must be independent (neither a subclass of the other),
// since callers need to distinguish a genuine timeout from a cancellation.
TEST(SerialMachineExceptions, TimeoutAndCancelledAreUnrelatedTypes) {
    EXPECT_FALSE((std::is_base_of_v<CommTimeoutError, OperationCancelledError>));
    EXPECT_FALSE((std::is_base_of_v<OperationCancelledError, CommTimeoutError>));
}

TEST(SerialMachineExceptions, CatchingCommTimeoutDoesNotCatchOperationCancelled) {
    try {
        throw OperationCancelledError("cancelled");
    } catch (const CommTimeoutError &) {
        FAIL() << "OperationCancelledError should not be caught as CommTimeoutError";
    } catch (const OperationCancelledError &e) {
        EXPECT_STREQ(e.what(), "cancelled");
    }
}

TEST(SerialMachineExceptions, CatchingOperationCancelledDoesNotCatchCommTimeout) {
    try {
        throw CommTimeoutError("timed out");
    } catch (const OperationCancelledError &) {
        FAIL() << "CommTimeoutError should not be caught as OperationCancelledError";
    } catch (const CommTimeoutError &e) {
        EXPECT_STREQ(e.what(), "timed out");
    }
}
