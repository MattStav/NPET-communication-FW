#ifndef NPET_COMM_CLI_H
#define NPET_COMM_CLI_H
#include "NPET_comm.h"
#include "cli.h"

constexpr std::string_view COMM_INIT = "Initializing NPET communication framework";
constexpr std::string_view COMM_CLOSE = "Closing NPET communication framework";


class NPET_comm_CLI : public NPET_comm {
    measurement raw_time_constant();

    __float128 measure_average_fraction(int aver_num, int channel_num);

    int calc_integer(int int_logic, int channel_num);

    /// Centralized error handler for CLI functions
    /// @tparam Func Function type
    /// @param func Function to execute
    /// @param func_name Name of the function for error reporting
    /// @return Result of the function or default value on error
    template<typename Func>
    auto safe_exec(Func &&func, const std::string &func_name) {
        SPDLOG_DEBUG("Executing {} with safety fallback ...", func_name);
        try {
            // Execute the func
            return func();
        } catch (const std::runtime_error &e) {
            cli::err("Error in " + func_name + ": " + e.what());
            // Return default values based on the expected return type
            using ReturnType = decltype(func());
            if constexpr (std::is_same_v<ReturnType, void>) { return; }
            if constexpr (std::is_same_v<ReturnType, bool>) { return false; }
            if constexpr (std::is_integral_v<ReturnType>) { return static_cast<ReturnType>(0); }
            // Return an invalid measurement
            if constexpr (std::is_same_v<ReturnType, measurement>) { return measurement{-2}; }
            return ReturnType{}; // Default constructor for other return types
        }
    }

public:
    void open_communication_CLI();

    [[nodiscard]] bool is_responsive_CLI();

    void set_FW_ver_CLI();

    void detect_FW_ver_CLI();

    void generate_pulses_CLI();

    void set_baud_rate_CLI();

    void read_batch_measurements_CLI();

    void set_time_constant_CLI();

    void reset_CLI();

    ///
    /// Constructor
    /// Handle the NPET communication initialization in CLI mode.
    /// Have the user select the COM port if it can't be auto-detected.
    /// Detect the NPET firmware version automatically.
    explicit NPET_comm_CLI() {
        SPDLOG_INFO(COMM_INIT);
        cli::echo(COMM_INIT.data(), fg::blue, style::bold);
        open_communication_CLI();
        detect_FW_ver_CLI();
    } // end of constructor

    /// Destructor
    ~NPET_comm_CLI() {
        SPDLOG_INFO(COMM_CLOSE);
        cli::echo(COMM_CLOSE.data(), fg::blue, style::bold);
        cli::echo("Baud rate will be reset to 115200 and COM port will be closed");
    } // end of destructor
};


#endif //NPET_COMM_CLI_H
