#ifndef LOGGING_H
#define LOGGING_H

#include <filesystem>

///
/// Get a path to the log directory.
/// @return Path to where logs are stored, which is in the APPDATA folder under NPET_FW/logs with a filename based on the current datetime.
std::filesystem::path getLogPath();

///
/// Initialize file logging using spdlog library.
void initLogging();

#endif // LOGGING_H
