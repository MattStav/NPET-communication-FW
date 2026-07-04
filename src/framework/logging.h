#ifndef LOGGING_H
#define LOGGING_H

#include <filesystem>

std::filesystem::path get_log_path();

void init_logging();

#endif // LOGGING_H
