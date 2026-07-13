#include "test_logging.h"

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
        spdlog::init_thread_pool(8192, 1); // create thread pool before init_logging
    }

    void TearDown() override {
        if (const auto logger = spdlog::get("Logger")) {
            logger->flush();
        }
        spdlog::drop("Logger");
        spdlog::shutdown(); // tears down thread pool cleanly
    }
};

TEST(GetLogPathTest, IsAbsolutePath) {
    EXPECT_TRUE(get_log_path().is_absolute());
}

TEST(GetLogPathTest, HasLogExtension) {
    EXPECT_EQ(get_log_path().extension(), ".log");
}

TEST(GetLogPathTest, IsUnderAppdata) {
    const char *appdata = std::getenv("APPDATA");
    EXPECT_NE(get_log_path().string().find(appdata), std::string::npos);
}

TEST(GetLogPathTest, IsUnderNPETLogsDirectory) {
    const auto path = get_log_path();
    const auto parent = path.parent_path();
    EXPECT_EQ(parent.filename(), "FW_logs");
    EXPECT_EQ(parent.parent_path(), USER_FILES);
}

TEST(GetLogPathTest, FilenameMatchesDatetimeFormat) {
    const auto filename = get_log_path().stem().string();
    const std::regex datetime_pattern(R"(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2})");
    EXPECT_TRUE(std::regex_match(filename, datetime_pattern));
}

TEST(GetLogPathTest, ReturnsSamePathOnRepeatedCalls) {
    // Static path must not change between calls
    EXPECT_EQ(get_log_path(), get_log_path());
}

TEST(GetLogPathTest, ParentDirectoryExists) {
    EXPECT_TRUE(std::filesystem::exists(get_log_path().parent_path()));
}

TEST_F(LoggingTest, CreatesLoggerNamedLogger) {
    init_logging();
    EXPECT_NE(spdlog::get("Logger"), nullptr);
}

TEST_F(LoggingTest, LoggerLevelIsDebug) {
    init_logging();
    EXPECT_EQ(spdlog::get("Logger")->level(), spdlog::level::debug);
}

TEST_F(LoggingTest, SecondCallDoesNotReinitialise) {
    init_logging();
    const auto first = spdlog::get("Logger");
    init_logging(); // should be a no-op
    const auto second = spdlog::get("Logger");
    EXPECT_EQ(first, second);
}

TEST_F(LoggingTest, CreatesLogFile) {
    init_logging();
    // Give the async logger a moment to flush
    spdlog::get("Logger")->flush();
    EXPECT_TRUE(std::filesystem::exists(get_log_path()));
}

TEST_F(LoggingTest, LogFileIsUnderExpectedDirectory) {
    init_logging();
    const auto log_path = get_log_path();
    EXPECT_EQ(log_path.parent_path().filename(), "FW_logs");
    EXPECT_EQ(log_path.parent_path().parent_path(), USER_FILES);
}
