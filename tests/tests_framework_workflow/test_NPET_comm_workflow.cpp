#include "test_NPET_comm_workflow.h"

#include <gtest/gtest.h>
#include <string>

// Tests below exercise NPETComm's protocol surface over the live connection brought up by
// FrameworkWorkflowFixture::SetUpTestSuite() (see test_workflow_fixture.h/.cpp): within one
// group, GoogleTest runs tests in source-declaration order, and TearDownTestSuite() re-checks
// isResponsive() right before closing, so a test that silently breaks the connection shows up as
// a teardown failure on its own group instead of quietly poisoning some later, unrelated test.

// Proves the setup itself: two virtual ports paired via com0com, a VirtualMachine listening on
// one end, and a plain NPETComm client on the other, actually exchange bytes end to end.
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

class CloseResetsBaudRateTest : public FrameworkWorkflowFixture {
};

// ~NPETComm() resets the device to the default 115200 baud rate before closing the port (see
// NPET_comm.h), so a client that leaves the device at a non-default rate doesn't strand the next
// thing that opens the port at some other speed.
TEST_F(CloseResetsBaudRateTest, DestructorResetsBaudRateTo115200) {
    ASSERT_TRUE(client->setBaudRate(230400));
    boost::asio::serial_port_base::baud_rate vm_baud_before{};
    vm->getPort().get_option(vm_baud_before);
    ASSERT_EQ(vm_baud_before.value(), 230400);

    client.reset(); // Triggers ~NPETComm()

    boost::asio::serial_port_base::baud_rate vm_baud_after{};
    vm->getPort().get_option(vm_baud_after);
    EXPECT_EQ(vm_baud_after.value(), 115200);
}

// The VM answers "?" with "Firmware none - offline", which detectFWVer() recognizes as the
// Virtual NPET firmware.
TEST_F(FrameworkWorkflowFixture, DetectFWVerSetsVirtualVersion) {
    client->detectFWVer();
    EXPECT_EQ(client->fw_version, FWVersion(FWVersion::VIRTUAL));
}

TEST_F(FrameworkWorkflowFixture, SetMeasuredDataFormatSucceedsForBinary) {
    EXPECT_TRUE(client->setMeasuredDataFormat(0));
}

TEST_F(FrameworkWorkflowFixture, SetMeasuredDataFormatSucceedsForAscii) {
    EXPECT_TRUE(client->setMeasuredDataFormat(1));
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


class SetFrequencyTest : public FrameworkWorkflowFixture,
                          public ::testing::WithParamInterface<FrequencyParams> {
};

TEST_P(SetFrequencyTest, SetFrequencySucceeds) {
    EXPECT_TRUE(client->setFrequency(GetParam().frequency));
}

INSTANTIATE_TEST_SUITE_P(
    Frequencies,
    SetFrequencyTest,
    ::testing::Values(
        FrequencyParams{"Minimum", 1},
        FrequencyParams{"Default", 100},
        FrequencyParams{"Typical", 500},
        FrequencyParams{"High", 5000}
    ),
    [](const ::testing::TestParamInfo<FrequencyParams> &info) { return info.param.name; }
);


class GeneratePulsesTest : public FrameworkWorkflowFixture,
                            public ::testing::WithParamInterface<PulseCountParams> {
};

TEST_P(GeneratePulsesTest, GeneratePulsesSucceeds) {
    EXPECT_TRUE(client->generatePulses(GetParam().num_of_pulses));
}

INSTANTIATE_TEST_SUITE_P(
    PulseCounts,
    GeneratePulsesTest,
    ::testing::Values(
        PulseCountParams{"Stop", 0},
        PulseCountParams{"Few", 5},
        PulseCountParams{"Many", 1000},
        PulseCountParams{"Infinite", -1}
    ),
    [](const ::testing::TestParamInfo<PulseCountParams> &info) { return info.param.name; }
);


class ExportedRawTimeConstantTest : public FrameworkWorkflowFixture,
                                     public ::testing::WithParamInterface<TimeConstantParams> {
};

TEST_P(ExportedRawTimeConstantTest, RoundTripsThroughImport) {
    const std::string RAW_CONST = Measurement{.intp = GetParam().intp, .fracp = GetParam().fracp}.toString();
    ASSERT_TRUE(client->exportTimeConstantRaw(RAW_CONST));
    EXPECT_EQ(client->importTimeConstantRaw(), RAW_CONST);
}

INSTANTIATE_TEST_SUITE_P(
    TimeConstants,
    ExportedRawTimeConstantTest,
    ::testing::Values(
        TimeConstantParams{"HalfSecond", 5, 0.5},
        TimeConstantParams{"LargeIntPart", 123456789, 0.125},
        TimeConstantParams{"ArbitraryDecimalFraction", 1, 0.0666},
        TimeConstantParams{"FullQuadPrecisionFraction", 1, strtoflt128("0.123456789123456", nullptr)}
    ),
    [](const ::testing::TestParamInfo<TimeConstantParams> &info) { return info.param.name; }
);


class ExportTimeConstantTest : public FrameworkWorkflowFixture,
                                public ::testing::WithParamInterface<TimeConstantParams> {
};

// Same round-trip idea as ExportedRawTimeConstantTest, but through the Measurement-typed overload.
TEST_P(ExportTimeConstantTest, RoundTripsThroughImport) {
    const Measurement CONSTANT{.meas_num = -1, .intp = GetParam().intp, .fracp = GetParam().fracp};
    EXPECT_TRUE(client->exportTimeConstant(CONSTANT));
    EXPECT_EQ(client->importTimeConstantRaw(), CONSTANT.toString());
}

INSTANTIATE_TEST_SUITE_P(
    TimeConstants,
    ExportTimeConstantTest,
    ::testing::Values(
        TimeConstantParams{"QuarterSecond", 42, 0.25},
        TimeConstantParams{"LargeIntPart", 1000000, 0.5},
        TimeConstantParams{"EighthSecond", 3, 0.125},
        TimeConstantParams{"FullQuadPrecisionFraction", 1, strtoflt128("0.123456789123456", nullptr)}
    ),
    [](const ::testing::TestParamInfo<TimeConstantParams> &info) { return info.param.name; }
);
