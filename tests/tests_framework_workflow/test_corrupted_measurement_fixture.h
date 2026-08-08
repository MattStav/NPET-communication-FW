#ifndef TEST_CORRUPTED_MEASUREMENT_FIXTURE_H
#define TEST_CORRUPTED_MEASUREMENT_FIXTURE_H

#include <gtest/gtest.h>
#include <memory>
#include <thread>

#include "NPET_comm.h"
#include "test_workflow_fixture.h"
#include "virtual_machine.h"

// How often the VM corrupts a measurement's checksum byte (every Nth packet sent within a single
// batch, 1-indexed - see VirtualMachine::sendMeasurements()). Chosen so a handful of small,
// exact batch sizes (e.g. 6, 9) divide evenly, making the expected corrupted count deterministic.
inline constexpr int CORRUPT_EVERY = 3;

class CorruptedMeasurementWorkflowFixture : public ::testing::Test {
protected:
    static std::unique_ptr<VirtualMachine> vm;
    static std::unique_ptr<std::jthread> vm_thread;
    static std::unique_ptr<NPETComm> client;

    static void SetUpTestSuite();

    static void TearDownTestSuite();

    void SetUp() override;
};

#endif //TEST_CORRUPTED_MEASUREMENT_FIXTURE_H
