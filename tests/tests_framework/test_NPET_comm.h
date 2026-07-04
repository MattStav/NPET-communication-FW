#ifndef TEST_NPET_COMM_H
#define TEST_NPET_COMM_H

#include <gtest/gtest.h>
#include "NPET_comm.h"

// Base fixture
class NPETCommFixture : public ::testing::Test {
protected:
    NPET_comm comm;
};

// Firmware test params
struct FirmwareVersionTestParams {
    int firmware_version;
};


#endif //TEST_NPET_COMM_H
