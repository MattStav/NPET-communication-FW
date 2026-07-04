#include "logging.h"

#include <windows.h>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

///
/// Get a path to the log directory.
/// @return Path to where logs are stored, which is in the APPDATA folder under NPET_FW/logs with a filename based on the current datetime.
std::filesystem::path get_log_path() {
    static const std::filesystem::path path = []() {
        char datetime[32];
        const std::time_t t = std::time(nullptr);
        std::strftime(datetime, sizeof(datetime), "%Y-%m-%d_%H-%M-%S", std::localtime(&t));
        auto p = std::filesystem::path(std::getenv("APPDATA")) / "NPET" / "logs" / (std::string(datetime) + ".log");
        std::filesystem::create_directories(p.parent_path());
        return p;
    }();  // immediately invoked lambda
    return path;
} // end of get_log_path function


/// Initialize file logging using spdlog library.
void init_logging() {
    assert(spdlog::thread_pool() != nullptr && "spdlog thread pool must be initialised before calling init_logging()");
    if (spdlog::get("Logger")) {
        SPDLOG_DEBUG("File logging already initiated");
        return;
    }
    const std::filesystem::path log_path = get_log_path();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        log_path.string(),
        true
    );
    file_sink->set_pattern("[%H:%M:%S.%e] [Thread: %t] [%l] [%s:%# %!] %v");
    file_sink->set_level(spdlog::level::debug);
    const auto logger = std::make_shared<spdlog::async_logger>(
        "Logger",
        file_sink,
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block
    );
    logger->set_level(spdlog::level::debug);
    spdlog::set_default_logger(logger);
    spdlog::flush_on(spdlog::level::info);
    spdlog::set_level(spdlog::level::debug);
    SPDLOG_DEBUG("Logging successfully initiated ...");
    SPDLOG_INFO("Logs saved to: {}", log_path.string());
} // end of init_file_logging function
