#include <gtest/gtest.h>
#include <filesystem>
#include <regex>
#include <spdlog/async.h>
#include <spdlog/spdlog.h>

#include "helper_func.h"
#include "logging.h"

class LoggingTest : public ::testing::Test {
protected:
    void SetUp() override {
        spdlog::init_thread_pool(8192, 1); // create thread pool before initLogging
    }

    void TearDown() override {
        if (const auto LOGGER = spdlog::get("Logger")) {
            LOGGER->flush();
        }
        spdlog::drop("Logger");
        spdlog::shutdown(); // tears down thread pool cleanly
    }
};

TEST(GetLogPathTest, IsAbsolutePath) {
    EXPECT_TRUE(getLogPath().is_absolute());
}

TEST(GetLogPathTest, HasLogExtension) {
    EXPECT_EQ(getLogPath().extension(), ".log");
}

TEST(GetLogPathTest, IsUnderAppdata) {
    const char *appdata = std::getenv("APPDATA");
    EXPECT_NE(getLogPath().string().find(appdata), std::string::npos);
}

TEST(GetLogPathTest, IsUnderNPETLogsDirectory) {
    const auto PATH = getLogPath();
    const auto PARENT = PATH.parent_path();
    EXPECT_EQ(PARENT.filename(), "FW_logs");
    EXPECT_EQ(PARENT.parent_path(), USER_FILES);
}

TEST(GetLogPathTest, FilenameMatchesDatetimeFormat) {
    const auto FILENAME = getLogPath().stem().string();
    const std::regex DATETIME_PATTERN(R"(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2})");
    EXPECT_TRUE(std::regex_match(FILENAME, DATETIME_PATTERN));
}

TEST(GetLogPathTest, ReturnsSamePathOnRepeatedCalls) {
    // Static path must not change between calls
    EXPECT_EQ(getLogPath(), getLogPath());
}

TEST(GetLogPathTest, ParentDirectoryExists) {
    EXPECT_TRUE(std::filesystem::exists(getLogPath().parent_path()));
}

TEST_F(LoggingTest, CreatesLoggerNamedLogger) {
    initLogging();
    EXPECT_NE(spdlog::get("Logger"), nullptr);
}

TEST_F(LoggingTest, LoggerLevelIsDebug) {
    initLogging();
    EXPECT_EQ(spdlog::get("Logger")->level(), spdlog::level::debug);
}

TEST_F(LoggingTest, SecondCallDoesNotReinitialise) {
    initLogging();
    const auto FIRST = spdlog::get("Logger");
    initLogging(); // should be a no-op
    const auto SECOND = spdlog::get("Logger");
    EXPECT_EQ(FIRST, SECOND);
}

TEST_F(LoggingTest, CreatesLogFile) {
    initLogging();
    // Give the async logger a moment to flush
    spdlog::get("Logger")->flush();
    EXPECT_TRUE(std::filesystem::exists(getLogPath()));
}

TEST_F(LoggingTest, LogFileIsUnderExpectedDirectory) {
    initLogging();
    const auto LOG_PATH = getLogPath();
    EXPECT_EQ(LOG_PATH.parent_path().filename(), "FW_logs");
    EXPECT_EQ(LOG_PATH.parent_path().parent_path(), USER_FILES);
}
