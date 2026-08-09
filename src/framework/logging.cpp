#include "logging.h"

#include <array>
#include <ctime>
#include <windows.h>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "helper_func.h"

std::filesystem::path getLogPath() {
    static const std::filesystem::path PATH = [] {
        std::array<char, 32> datetime{};
        const std::time_t T = std::time(nullptr);
        std::tm tm{};
        localtime_s(&tm, &T);
        std::strftime(datetime.data(), datetime.size(), "%Y-%m-%d_%H-%M-%S", &tm);
        auto p = USER_FILES / "FW_logs" / (std::string(datetime.data()) + ".log");
        std::filesystem::create_directories(p.parent_path());
        return p;
    }();  // immediately invoked lambda
    return PATH;
} // end of get_log_path function


void initLogging() {
    assert(spdlog::thread_pool() != nullptr && "spdlog thread pool must be initialised before calling init_logging()");
    if (spdlog::get("Logger")) {
        SPDLOG_DEBUG("File logging already initiated");
        return;
    }
    const std::filesystem::path LOG_PATH = getLogPath();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        LOG_PATH.string(),
        true
    );
    file_sink->set_pattern("[%H:%M:%S.%e] [Thread: %t] [%l] [%s:%# %!] %v");
    file_sink->set_level(spdlog::level::debug);
    const auto LOGGER = std::make_shared<spdlog::async_logger>(
        "Logger",
        file_sink,
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block
    );
    LOGGER->set_level(spdlog::level::debug);
    spdlog::set_default_logger(LOGGER);
    spdlog::flush_on(spdlog::level::info);
    spdlog::set_level(spdlog::level::debug);
    SPDLOG_DEBUG("Logging successfully initiated ...");
    SPDLOG_INFO("Logs saved to: {}", LOG_PATH.string());
} // end of initLogging function
