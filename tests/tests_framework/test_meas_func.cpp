#include "test_meas_func.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include "fw_version.h"
#include "gmock/gmock-matchers.h"

namespace fs = std::filesystem;
using ::testing::MatchesRegex;

TEST(OutputDirTest, EndsWithOutputs) {
    EXPECT_EQ(OUTPUT_DIR_NAME, "FW_outputs");
}

class XorChecksumTest : public testing::TestWithParam<XorChecksumParams> {
};

TEST_P(XorChecksumTest, ReturnsExpectedResult) {
    EXPECT_EQ(xorChecksum(GetParam().input), GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(
    XorChecksumTests,
    XorChecksumTest,
    testing::Values(
        // All zeros
        XorChecksumParams{{}, 0x00},
        // Single byte at various positions
        XorChecksumParams{{0xAB}, 0xAB},
        XorChecksumParams{{0, 0xAB}, 0xAB},
        XorChecksumParams{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xAB}, 0xAB},
        // Byte 12 is not included
        XorChecksumParams{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF}, 0x00},
        // Two identical bytes cancel
        XorChecksumParams{{0xAB, 0xAB}, 0x00},
        XorChecksumParams{{0xFF, 0xFF}, 0x00},
        // Even count of identical bytes cancels
        XorChecksumParams{{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 0x00},
        // Odd count of identical bytes leaves one
        XorChecksumParams{{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 0x01},
        // Known values
        XorChecksumParams{{1, 11}, 0x0A},
        XorChecksumParams{{1, 11, 5}, 0x0F},
        // All 0xFF in first 12 bytes
        XorChecksumParams{{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 0x00},
        // All 0xFF including byte 12 — result unchanged
        XorChecksumParams{{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 0x00},
        // Alternating bytes
        XorChecksumParams{{0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F,
        0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F}, 0x00},
        // All bytes distinct
        XorChecksumParams{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
        uint8_t(1^2^3^4^5^6^7^8^9^10^11^12)}
    )
);

class GetMeasurementCmdTest : public testing::TestWithParam<GetMeasurementCmdParams> {
};

TEST_P(GetMeasurementCmdTest, ReturnsExpectedResult) {
    const auto &p = GetParam();
    EXPECT_EQ(getMeasurementCmd(p.channel, p.num), p.expected);
}

INSTANTIATE_TEST_SUITE_P(
    GetMeasurementCmdTests,
    GetMeasurementCmdTest,
    testing::Values(
        // Channel 1 (e)
        GetMeasurementCmdParams{Channel::CH1, 1, "e1"},
        GetMeasurementCmdParams{Channel::CH1, 5, "e5"},
        GetMeasurementCmdParams{Channel::CH1, 10, "e10"},
        GetMeasurementCmdParams{Channel::CH1, 99, "e99"},
        // Channel 2 (h)
        GetMeasurementCmdParams{Channel::CH2, 1, "h1"},
        GetMeasurementCmdParams{Channel::CH2, 10, "h10"},
        GetMeasurementCmdParams{Channel::CH2, 99, "h99"}
    )
);

class ProcessMeasurementInvalidHeader : public testing::TestWithParam<ProcessMeasurementInvalidHeaderParams> {
};

TEST_P(ProcessMeasurementInvalidHeader, Throws) {
    EXPECT_THROW(
        decodeMeasurementSet(GetParam().input, 1e-8),
        std::invalid_argument
    );
}

INSTANTIATE_TEST_SUITE_P(
    ProcessMeasurementTests,
    ProcessMeasurementInvalidHeader,
    testing::Values(
        // Invalid first byte
        ProcessMeasurementInvalidHeaderParams{setHeader(0, 11)},
        ProcessMeasurementInvalidHeaderParams{setHeader(2, 11)},
        ProcessMeasurementInvalidHeaderParams{setHeader(255, 11)},
        // Invalid second byte
        ProcessMeasurementInvalidHeaderParams{setHeader(1, 0)},
        ProcessMeasurementInvalidHeaderParams{setHeader(1, 10)},
        ProcessMeasurementInvalidHeaderParams{setHeader(1, 12)},
        ProcessMeasurementInvalidHeaderParams{setHeader(1, 255)},
        // Both bytes invalid
        ProcessMeasurementInvalidHeaderParams{setHeader(0, 0)},
        ProcessMeasurementInvalidHeaderParams{setHeader(2, 10)},
        ProcessMeasurementInvalidHeaderParams{setHeader(255, 255)}
    )
);

TEST(ProcessMeasurement, InvalidChecksumThrows) {
    // Corrupt by flipping all bits
    {
        auto arr = makeZeroPacket(0);
        arr[12] ^= 0xFF;
        EXPECT_THROW(decodeMeasurementSet(arr, 1e-8), std::runtime_error);
    }
    // Corrupt by flipping one bit
    {
        auto arr = makeZeroPacket(0);
        arr[12] ^= 0x01;
        EXPECT_THROW(decodeMeasurementSet(arr, 1e-8), std::runtime_error);
    }
    // Corrupt by incrementing
    {
        auto arr = makeZeroPacket(0);
        arr[12] += 1;
        EXPECT_THROW(decodeMeasurementSet(arr, 1e-8), std::runtime_error);
    }
    // Corrupt by zeroing
    {
        auto arr = makeZeroPacket(0);
        arr[12] = 0x00;
        EXPECT_THROW(decodeMeasurementSet(arr, 1e-8), std::runtime_error);
    }
    // Corrupt by setting to max
    {
        auto arr = makeZeroPacket(0);
        arr[12] = 0xFF;
        EXPECT_THROW(decodeMeasurementSet(arr, 1e-8), std::runtime_error);
    }
}

class ProcessMeasurementMeasNum : public testing::TestWithParam<MeasNumParams> {
};

TEST_P(ProcessMeasurementMeasNum, ExtractedFromByte2) {
    const auto arr = makeZeroPacket(GetParam().meas_num);
    const Measurement result = decodeMeasurementSet(arr, 1e-8);
    EXPECT_EQ(result.meas_num, GetParam().meas_num);
}

INSTANTIATE_TEST_SUITE_P(
    ProcessMeasurementTests,
    ProcessMeasurementMeasNum,
    testing::Values(
        MeasNumParams{0},
        MeasNumParams{1},
        MeasNumParams{42},
        MeasNumParams{127},
        MeasNumParams{255}
    )
);

TEST(ProcessMeasurement, ZeroDataPacketGivesZeroMeasurement) {
    const auto arr = makeZeroPacket(1);
    const Measurement result = decodeMeasurementSet(arr, static_cast<__float128>(1e-8));
    EXPECT_EQ(result.intp, 0);
    EXPECT_NEAR(static_cast<double>(result.fracp), 0.0, 1e-12);
}

class ProcessMeasurementTimeConstant : public testing::TestWithParam<TimeConstantParams> {
};

TEST_P(ProcessMeasurementTimeConstant, AppliedToZeroPacket) {
    const auto &p = GetParam();
    auto arr = makeZeroPacket(1);
    const Measurement result = decodeMeasurementSet(arr, 1e-8, p.time_const);
    EXPECT_EQ(result.intp, p.expected_intp);
    EXPECT_NEAR(static_cast<double>(result.fracp), p.expected_fracp, 1e-15);
}

INSTANTIATE_TEST_SUITE_P(
    ProcessMeasurementTests,
    ProcessMeasurementTimeConstant,
    testing::Values(
        // No time constant (default)
        TimeConstantParams{{-1, 0, static_cast<__float128>(0.0)}, 0, 0.0},
        // Positive time constant
        TimeConstantParams{{-1, 5, static_cast<__float128>(0.25)}, 5, 0.25},
        TimeConstantParams{{-1, 3, static_cast<__float128>(0.5)}, 3, 0.5},
        TimeConstantParams{{-1, 0, static_cast<__float128>(0.5)}, 0, 0.5},
        // Negative fracp — resolve() must be applied
        TimeConstantParams{{-1, 3, static_cast<__float128>(-0.3)}, 2, 0.7},
        TimeConstantParams{{-1, 1, static_cast<__float128>(-0.5)}, 0, 0.5},
        // fracp carries into intp
        TimeConstantParams{{-1, 3, static_cast<__float128>(1.3)}, 4, 0.3}
    )
);

TEST(ProcessMeasurement, MultiplierFW1VsFW2DiffersForNonZeroPacket) {
    // Use a packet with a non-zero mid-word to make multiplier matter
    std::array<uint8_t, 13> arr = {1, 11, 1, 0, 0, 0, 100, 0, 0, 0xFF, 0xFF, 0x3F, 0};
    arr[12] = xorChecksum(arr);
    const __float128 mult1 = FWVersion(FWVersion::ORIGINAL).getMultiplier();
    const __float128 mult2 = FWVersion(FWVersion::AD_REVISION).getMultiplier();
    const Measurement r1 = decodeMeasurementSet(arr, mult1);
    const Measurement r2 = decodeMeasurementSet(arr, mult2);
    EXPECT_NE(r1.fracp, r2.fracp);
}

class EncodeDecodeRoundTripTest : public testing::TestWithParam<EncodeDecodeRoundTripParams> {
};

TEST_P(EncodeDecodeRoundTripTest, DecodesBackToRequestedTime) {
    const auto &p = GetParam();
    const Measurement INPUT{.meas_num = p.meas_num, .intp = p.seconds, .fracp = p.fracp};
    const auto bytes = encodeMeasurementSet(INPUT, p.multiplier);
    const Measurement RESULT = decodeMeasurementSet(bytes, p.multiplier);
    EXPECT_EQ(RESULT.meas_num, p.meas_num);
    const double expected_total = static_cast<double>(p.seconds) + static_cast<double>(p.fracp);
    const double actual_total = static_cast<double>(RESULT.intp) + static_cast<double>(RESULT.fracp);
    // The wire format's finest step is ~152.588 fs (1e-8 / 2^16 s); allow a bit of slack for rounding at both
    // encode and decode. Comparing the combined total (rather than intp/fracp separately) sidesteps cases where
    // the requested time sits within one tick of a whole-second boundary and legitimately rounds the other way.
    EXPECT_NEAR(actual_total, expected_total, 2e-13);
}

INSTANTIATE_TEST_SUITE_P(
    EncodeDecodeRoundTripTests,
    EncodeDecodeRoundTripTest,
    testing::Values(
        // Zero time
        EncodeDecodeRoundTripParams{1, 0, static_cast<__float128>(0.0), static_cast<__float128>(1e-8)},
        // Fractional seconds at various points
        EncodeDecodeRoundTripParams{1, 0, static_cast<__float128>(0.5), static_cast<__float128>(1e-8)},
        // Sub-LSB input: finer than the format can represent, must round to the nearest tick, not throw or drift
        EncodeDecodeRoundTripParams{1, 0, static_cast<__float128>(1e-15), static_cast<__float128>(1e-8)},
        // Fractional part right at the top of its range, next to a whole-second boundary
        EncodeDecodeRoundTripParams{1, 0, static_cast<__float128>(0.999999999999999), static_cast<__float128>(1e-8)},
        // Non-zero seconds with an arbitrary femtosecond-precision fraction
        EncodeDecodeRoundTripParams{5, 5, static_cast<__float128>(0.123456789012345), static_cast<__float128>(1e-8)},
        EncodeDecodeRoundTripParams{42, 1000000, static_cast<__float128>(0.999999999999999), static_cast<__float128>(1e-8)},
        // Different meas_num values
        EncodeDecodeRoundTripParams{0, 10, static_cast<__float128>(0.0), static_cast<__float128>(1e-8)},
        EncodeDecodeRoundTripParams{255, 10, static_cast<__float128>(0.0), static_cast<__float128>(1e-8)},
        // FW1 multiplier (2e-8) rather than FW2/3's default
        EncodeDecodeRoundTripParams{1, 100, static_cast<__float128>(0.25), static_cast<__float128>(2e-8)},
        EncodeDecodeRoundTripParams{1, 0, static_cast<__float128>(0.0), static_cast<__float128>(2e-8)}
    )
);

TEST(EncodeMeasurementSet, ZeroTimeMatchesKnownZeroPacket) {
    // encodeMeasurementSet(_, 0, 0, _) should land exactly on the fine field's zero point (4194303 = 0x3FFFFF,
    // little-endian across bytes 9..11), which is the same magic packet makeZeroPacket() builds by hand above.
    constexpr Measurement INPUT{.meas_num = 1, .intp = 0, .fracp = static_cast<__float128>(0.0)};
    const auto bytes = encodeMeasurementSet(INPUT, static_cast<__float128>(1e-8));
    EXPECT_EQ(bytes, makeZeroPacket(1));
}

TEST(EncodeMeasurementSet, PreservesWholeSecondsAwayFromBoundary) {
    constexpr Measurement INPUT{.meas_num = 1, .intp = 5, .fracp = static_cast<__float128>(0.5)}; // 5.5 s
    const auto bytes = encodeMeasurementSet(INPUT, static_cast<__float128>(1e-8));
    const Measurement result = decodeMeasurementSet(bytes, static_cast<__float128>(1e-8));
    EXPECT_EQ(result.intp, 5);
    EXPECT_NEAR(static_cast<double>(result.fracp), 0.5, 2e-13);
}

TEST(EncodeMeasurementSet, HeaderBytesAreFixed) {
    constexpr Measurement INPUT{.meas_num = 3, .intp = 42, .fracp = static_cast<__float128>(0.0)};
    const auto bytes = encodeMeasurementSet(INPUT, static_cast<__float128>(1e-8));
    EXPECT_EQ(bytes[0], 1);
    EXPECT_EQ(bytes[1], 11);
}

TEST(EncodeMeasurementSet, ChecksumIsValid) {
    constexpr Measurement INPUT{.meas_num = 7, .intp = 12345, .fracp = static_cast<__float128>(0.67890123456789)};
    const auto bytes = encodeMeasurementSet(INPUT, static_cast<__float128>(1e-8));
    EXPECT_EQ(xorChecksum(bytes), bytes[12]);
}

TEST(EncodeMeasurementSet, MeasNumStoredInByteTwo) {
    for (const uint8_t n : {0, 1, 42, 200, 255}) {
        const Measurement INPUT{.meas_num = n, .intp = 0, .fracp = static_cast<__float128>(0.0)};
        const auto bytes = encodeMeasurementSet(INPUT, static_cast<__float128>(1e-8));
        EXPECT_EQ(bytes[2], n);
    }
}

class EncodeInvalidInputTest : public testing::TestWithParam<EncodeInvalidInputParams> {
};

TEST_P(EncodeInvalidInputTest, ThrowsInvalidArgument) {
    const auto &p = GetParam();
    const Measurement INPUT{.meas_num = 0, .intp = p.seconds, .fracp = p.fracp};
    EXPECT_THROW(
        encodeMeasurementSet(INPUT, static_cast<__float128>(1e-8)),
        std::invalid_argument
    );
}

INSTANTIATE_TEST_SUITE_P(
    EncodeInvalidInputTests,
    EncodeInvalidInputTest,
    testing::Values(
        EncodeInvalidInputParams{-1, static_cast<__float128>(0.0)}, // negative seconds
        EncodeInvalidInputParams{0, static_cast<__float128>(-0.5)}, // negative fracp
        EncodeInvalidInputParams{0, static_cast<__float128>(1.0)}, // fracp must be < 1
        EncodeInvalidInputParams{0, static_cast<__float128>(5.0)}
    )
);

TEST(EncodeMeasurementSet, TimeTooLargeThrowsOutOfRange) {
    // Comfortably past the ~2.81e6 s representable ceiling for multiplier = 1e-8 (2^48 - 1 combined ticks)
    const Measurement INPUT{.meas_num = 0, .intp = 3000000, .fracp = static_cast<__float128>(0.0)};
    EXPECT_THROW(
        encodeMeasurementSet(INPUT, static_cast<__float128>(1e-8)),
        std::out_of_range
    );
}

class Float128ToStringTest : public testing::TestWithParam<Float128ToStringParams> {
};

TEST_P(Float128ToStringTest, ReturnsExpectedString) {
    EXPECT_EQ(float128ToString(GetParam().input), GetParam().expected);
}

TEST_P(Float128ToStringTest, Has15DecimalDigits) {
    const std::string s = float128ToString(GetParam().input);
    const auto dot = s.find('.');
    ASSERT_NE(dot, std::string::npos);
    EXPECT_EQ(s.size() - dot - 1, 15u);
}

TEST_P(Float128ToStringTest, ContainsDecimalPoint) {
    EXPECT_NE(float128ToString(GetParam().input).find('.'), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(
    Float128ToStringTests,
    Float128ToStringTest,
    testing::Values(
        Float128ToStringParams{static_cast<__float128>( 0.0), "0.000000000000000"},
        Float128ToStringParams{static_cast<__float128>( 1.0), "1.000000000000000"},
        Float128ToStringParams{static_cast<__float128>(-1.0), "-1.000000000000000"},
        Float128ToStringParams{static_cast<__float128>( 1.5), "1.500000000000000"},
        Float128ToStringParams{static_cast<__float128>(42.0), "42.000000000000000"},
        Float128ToStringParams{static_cast<__float128>( 0.123456789012345), "0.123456789012345"},
        Float128ToStringParams{static_cast<__float128>(-0.5), "-0.500000000000000"},
        Float128ToStringParams{static_cast<__float128>(0.999999999999999),"0.999999999999999"}
    )
);


// ---------------------------------------------------------------
// Test fixture – cleans up the temp directory before/after each
// test so tests are hermetic.
// ---------------------------------------------------------------
class OutputFileNameTest : public ::testing::Test {
protected:
    fs::path base_dir;

    void SetUp() override {
        const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();

        base_dir = fs::temp_directory_path()
                 / "np_test"
                 / info->test_suite_name()
                 / info->name();
        std::error_code ec;
        fs::remove_all(base_dir, ec);
        ec.clear();
        fs::create_directories(base_dir, ec);
        ASSERT_FALSE(ec) << "Failed to create test directory: " << base_dir
                         << " error: " << ec.message();
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(base_dir, ec);
    }

    // Helper – extract just the filename (no directory) from a full path
    static std::string basename(const std::string &path) {
        return fs::path(path).filename().string();
    }

    // Helper – return a regex that matches the expected filename pattern
    // for a given channel: EPOCH<channel>_YYYYMMDD_HHMMSS.out
    static std::string make_pattern(const Channel channel) {
        return "EPOCH" + std::to_string(static_cast<int>(channel)) +
               R"(_\d\d\d\d\d\d\d\d_\d\d\d\d\d\d\.out)";
    }
};


class FilenameFormatTest
        : public OutputFileNameTest,
          public ::testing::WithParamInterface<Channel> {
};

INSTANTIATE_TEST_SUITE_P(ChannelVariants, FilenameFormatTest, ::testing::Values(Channel::CH1, Channel::CH2));

// The returned path must end with the correct filename pattern
TEST_P(FilenameFormatTest, FilenameMatchesPattern) {
    const Channel channel = GetParam();
    const std::string result = outputFilePath(channel, base_dir);
    const std::string fname = basename(result);
    EXPECT_THAT(fname, MatchesRegex(make_pattern(channel)))
            << "Unexpected filename for channel " << static_cast<int>(channel)
            << ": " << fname;
}

// The file extension must always be ".out"
TEST_P(FilenameFormatTest, ExtensionIsOut) {
    const Channel channel = GetParam();
    const fs::path result(outputFilePath(channel, base_dir));
    EXPECT_EQ(result.extension().string(), ".out")
        << "Wrong extension for channel " << static_cast<int>(channel);
}

// The stem must start with "EPOCH" followed by the channel number
TEST_P(FilenameFormatTest, StemStartsWithEpochAndChannel) {
    const Channel channel = GetParam();
    const fs::path result(outputFilePath(channel, base_dir));
    const std::string stem = result.stem().string(); // e.g. "EPOCH7_20240101_120000"
    const std::string expected_prefix = "EPOCH" + std::to_string(static_cast<int>(channel)) + "_";
    EXPECT_EQ(stem.substr(0, expected_prefix.size()), expected_prefix)
        << "Stem does not start with expected prefix for channel " << static_cast<int>(channel);
}

// The timestamp embedded in the stem must be a valid YYYYMMDD_HHMMSS string
TEST_P(FilenameFormatTest, EmbeddedTimestampIsValid) {
    const Channel channel = GetParam();
    const fs::path result(outputFilePath(channel, base_dir));
    const std::string stem = result.stem().string();
    // Strip "EPOCH<channel>_" prefix to isolate the timestamp
    const std::string prefix = "EPOCH" + std::to_string(static_cast<int>(channel)) + "_";
    ASSERT_GE(stem.size(), prefix.size());
    const std::string ts = stem.substr(prefix.size()); // "YYYYMMDD_HHMMSS"
    ASSERT_EQ(ts.size(), 15u) << "Timestamp length mismatch: " << ts;
    // Structural checks: digits and underscore in the right places
    for (int i = 0; i < 8; ++i)
        EXPECT_TRUE(std::isdigit(ts[i])) << "Non-digit at pos " << i << " in " << ts;
    EXPECT_EQ(ts[8], '_') << "Expected underscore at pos 8 in " << ts;
    for (int i = 9; i < 15; ++i)
        EXPECT_TRUE(std::isdigit(ts[i])) << "Non-digit at pos " << i << " in " << ts;
    // Range checks on the date/time components
    const int year = std::stoi(ts.substr(0, 4));
    const int month = std::stoi(ts.substr(4, 2));
    const int day = std::stoi(ts.substr(6, 2));
    const int hour = std::stoi(ts.substr(9, 2));
    const int minute = std::stoi(ts.substr(11, 2));
    const int second = std::stoi(ts.substr(13, 2));
    EXPECT_GE(year, 2024);
    EXPECT_GE(month, 1);
    EXPECT_LE(month, 12);
    EXPECT_GE(day, 1);
    EXPECT_LE(day, 31);
    EXPECT_GE(hour, 0);
    EXPECT_LE(hour, 23);
    EXPECT_GE(minute, 0);
    EXPECT_LE(minute, 59);
    EXPECT_GE(second, 0);
    EXPECT_LE(second, 59);
}

class DirectoryCreationTest : public OutputFileNameTest {
};

// If the directory does not exist, calling the function must create it
TEST_F(DirectoryCreationTest, CreatesDirectoryWhenAbsent) {
    const fs::path output_dir = base_dir / OUTPUT_DIR_NAME;
    ASSERT_FALSE(fs::exists(output_dir)) << "Precondition: dir should not exist yet";
    outputFilePath(Channel::CH1, base_dir);
    EXPECT_TRUE(fs::exists(output_dir)) << "Directory was not created";
    EXPECT_TRUE(fs::is_directory(output_dir)) << "Path exists but is not a directory";
}

// If the directory already exists, the function must not throw or fail
TEST_F(DirectoryCreationTest, DoesNotFailWhenDirectoryAlreadyExists) {
    fs::create_directories(base_dir);
    ASSERT_TRUE(fs::exists(base_dir));
    EXPECT_NO_THROW(outputFilePath(Channel::CH1, base_dir));
}

// Multiple successive calls must not recreate / destroy the directory
TEST_F(DirectoryCreationTest, DirectoryStillExistsAfterMultipleCalls) {
    outputFilePath(Channel::CH1, base_dir);
    outputFilePath(Channel::CH1, base_dir);
    outputFilePath(Channel::CH2, base_dir);
    EXPECT_TRUE(fs::is_directory(base_dir));
}

// Existing files inside the directory are preserved across calls
TEST_F(DirectoryCreationTest, ExistingFilesInDirectoryArePreserved) {
    fs::create_directories(base_dir);
    const fs::path sentinel = base_dir / "sentinel.txt"; {
        std::ofstream f(sentinel);
        f << "keep me";
    }
    ASSERT_TRUE(fs::exists(sentinel));
    outputFilePath(Channel::CH1, base_dir);
    EXPECT_TRUE(fs::exists(sentinel)) << "Pre-existing file was removed";
}


class ReturnValueTest : public OutputFileNameTest {
};

// Returned path must include the output directory as its parent
TEST_F(ReturnValueTest, ParentDirectoryMatchesOutputDir) {
    const fs::path result(outputFilePath(Channel::CH1, base_dir));
    EXPECT_EQ(result.parent_path(), base_dir / OUTPUT_DIR_NAME)
        << "Parent path: " << result.parent_path();
}

// The returned string must be non-empty
TEST_F(ReturnValueTest, ReturnValueIsNonEmpty) {
    EXPECT_FALSE(outputFilePath(Channel::CH1, base_dir).empty());
}

// The returned path must use the native directory separator (i.e., be a
// well-formed path, not a bare filename)
TEST_F(ReturnValueTest, ReturnValueContainsDirectorySeparator) {
    const std::string result = outputFilePath(Channel::CH1, base_dir);
    const bool has_sep =
            result.find('/') != std::string::npos ||
            result.find('\\') != std::string::npos;
    EXPECT_TRUE(has_sep) << "No directory separator found in: " << result;
}

TEST_F(ReturnValueTest, PathContainsExpectedDirectoryAndFilename) {
    const fs::path result(outputFilePath(Channel::CH1, base_dir));
    EXPECT_FALSE(result.filename().empty());
    EXPECT_EQ(result.parent_path().filename(), OUTPUT_DIR_NAME);
}

TEST_F(ReturnValueTest, DoesNotCreateFile) {
    const fs::path result(outputFilePath(Channel::CH1, base_dir));

    EXPECT_FALSE(fs::exists(result));
    EXPECT_TRUE(fs::exists(base_dir));
}


class UniquenessTest : public OutputFileNameTest {
};

// Two calls with different channels in the same second must return
// different filenames
TEST_F(UniquenessTest, DifferentChannelsProduceDifferentNames) {
    const std::string a = outputFilePath(Channel::CH1, base_dir);
    const std::string b = outputFilePath(Channel::CH2, base_dir);
    EXPECT_NE(a, b);
}

// Calls separated by at least one second must produce different names
// for the same channel (timestamp uniqueness)
TEST_F(UniquenessTest, SameChannelDifferentSecondsProduceDifferentNames) {
    const std::string first = outputFilePath(Channel::CH1, base_dir);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const std::string second = outputFilePath(Channel::CH1, base_dir);
    EXPECT_NE(first, second)
        << "Expected different names after 1-second sleep";
}


class TimestampProximityTest : public OutputFileNameTest {
};

// The timestamp baked into the filename must be within ±2 seconds of
// the time at which the function was called.
TEST_F(TimestampProximityTest, TimestampIsCloseToCallTime) {
    const std::time_t before = std::time(nullptr);
    const std::string result = outputFilePath(Channel::CH1, base_dir);
    const std::time_t after = std::time(nullptr);
    const fs::path p(result);
    const std::string stem = p.stem().string(); // EPOCH0_YYYYMMDD_HHMMSS
    // Isolate the timestamp portion
    const std::string ts = stem.substr(std::string("EPOCH0_").size());
    std::tm tm_result{};
    // Parse YYYYMMDD_HHMMSS manually
    tm_result.tm_year = std::stoi(ts.substr(0, 4)) - 1900;
    tm_result.tm_mon = std::stoi(ts.substr(4, 2)) - 1;
    tm_result.tm_mday = std::stoi(ts.substr(6, 2));
    tm_result.tm_hour = std::stoi(ts.substr(9, 2));
    tm_result.tm_min = std::stoi(ts.substr(11, 2));
    tm_result.tm_sec = std::stoi(ts.substr(13, 2));
    tm_result.tm_isdst = -1;
    const std::time_t ts_time = std::mktime(&tm_result);
    EXPECT_GE(ts_time, before - 2)
        << "Timestamp predates the call by more than 2 seconds";
    EXPECT_LE(ts_time, after + 2)
        << "Timestamp is more than 2 seconds in the future relative to the call";
}
