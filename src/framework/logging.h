#ifndef LOGGING_H
#define LOGGING_H

#include <filesystem>

std::filesystem::path getLogPath();

void initLogging();

#endif // LOGGING_H
