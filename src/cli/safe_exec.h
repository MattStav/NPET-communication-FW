#ifndef NPET_COMM_FW_SAFE_EXEC_H
#define NPET_COMM_FW_SAFE_EXEC_H
#include <string>
#include <type_traits>
#include <utility>
#include <spdlog/spdlog.h>

#include "cli.h"
#include "meas_func.h"

///
/// Centralized error handler for CLI functions.
/// Shared by NPETCommCLI and NPETDualCLI so both can wrap framework calls
/// with the same fallback/logging behavior.
/// @tparam Func Function type
/// @param func Function to execute
/// @param func_name Name of the function for error reporting
/// @return Result of the function or default value on error
template<typename Func>
auto safeExec(Func &&func, const std::string &func_name) {
    SPDLOG_DEBUG("Executing {} with safety fallback ...", func_name);
    try {
        // Execute the func
        return std::forward<Func>(func)();
    } catch (const std::runtime_error &e) {
        Cli::err("Error in " + func_name + ": " + e.what());
        // Return default values based on the expected return type
        using ReturnType = decltype(func());
        if constexpr (std::is_same_v<ReturnType, void>) { return; }
        if constexpr (std::is_same_v<ReturnType, bool>) { return false; }
        if constexpr (std::is_integral_v<ReturnType>) { return static_cast<ReturnType>(0); }
        // Return an invalid measurement
        if constexpr (std::is_same_v<ReturnType, Measurement>) { return Measurement{.meas_num = -2}; }
        return ReturnType{}; // Default constructor for other return types
    }
}

#endif //NPET_COMM_FW_SAFE_EXEC_H
