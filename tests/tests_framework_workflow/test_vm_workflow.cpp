#include "test_workflow_fixture.h"

#include <csignal>
#include <future>
#include <gtest/gtest.h>

/// Simulates Ctrl+C via std::raise(SIGINT): on Windows.
TEST(VirtualMachineTest, DeviceLoopTerminatesOnSigint) {
    VirtualMachine vm{VmConfig{}};
    try {
        vm.openCommunication(VM_COM_PORT, BAUD_RATE);
    } catch (const std::exception &e) {
        GTEST_SKIP() << "Could not open COM" << VM_COM_PORT << ": " << e.what()
                << ". This test requires a com0com virtual null-modem pair on COM"
                << VM_COM_PORT << "/COM" << CLIENT_COM_PORT << " - skipping.";
    }
    std::future<void> loop_done = std::async(std::launch::async, [&vm] { vm.deviceLoop(); });
    // Give deviceLoop() time to install its signal_set and enter its first blocking read;
    // raising the signal any earlier risks cancel() firing with nothing yet pending to cancel.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::raise(SIGINT);
    const auto STATUS = loop_done.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(STATUS, std::future_status::ready) << "deviceLoop() did not return within 5s of SIGINT";
    if (STATUS == std::future_status::ready) {
        EXPECT_NO_THROW(loop_done.get());
    } else {
        // Don't let the background task outlive this test (and the local vm it captured by reference).
        vm.getPort().cancel();
        loop_done.wait();
    }
    vm.closeCommunication();
    EXPECT_FALSE(vm.isOpen());
}


class MeasurementCounterTest : public FrameworkWorkflowFixture {
};

/// VirtualMachine::measurement_counter_ starts at 0 on construction,
/// so the first measurement read from a freshly initialized VM must carry meas_num == 1
TEST_F(MeasurementCounterTest, FirstMeasurementHasCounterOne) {
    client->setFWVer(FWVersion(FWVersion::VIRTUAL)); // must match the multiplier the VM encoded with
    const Measurement FIRST = client->readSingleMeasurement(Channel::CH1);
    EXPECT_EQ(FIRST.meas_num, 1);
}


class MeasurementStartTimeTest : public FrameworkWorkflowFixture {
};

/// Each measurement's elapsed time is measured against START_TIME, captured when the VM is
/// constructed, so a measurement read right after startup should report 0 whole seconds elapsed.
/// Channel 1 (10ms tick grid at 100Hz) is used rather than channel 2 (fixed 1s grid), since
/// channel 2's very first tick always lands at exactly 1.0s, not 0.
TEST_F(MeasurementStartTimeTest, FirstMeasurementStartsAtZeroSeconds) {
    client->setFWVer(FWVersion(FWVersion::VIRTUAL)); // must match the multiplier the VM encoded with
    const Measurement FIRST = client->readSingleMeasurement(Channel::CH1);
    EXPECT_EQ(FIRST.intp, 0);

    // Recheck after a real delay: elapsed time should have actually advanced against the same
    // START_TIME, not stayed pinned at 0 or reset on the next read.
    std::this_thread::sleep_for(std::chrono::seconds(2));
    const Measurement SECOND = client->readSingleMeasurement(Channel::CH1);
    EXPECT_GE(SECOND.intp, 2);
}
