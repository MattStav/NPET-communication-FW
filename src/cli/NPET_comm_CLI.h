#ifndef NPET_COMM_CLI_H
#define NPET_COMM_CLI_H
#include <cstdint>
#include <utility>

#include "NPET_comm.h"
#include "cli.h"

constexpr std::string_view COMM_INIT = "Initializing NPET communication framework";
constexpr std::string_view COMM_CLOSE = "Closing NPET communication framework";


class NPETCommCLI : public NPETComm {
    const int DEFAULT_BAUD_RATE = 115200;

    Measurement rawTimeConstant();

    /// Logic used to define the integer part of the time correction constant.
    enum class IntLogic : std::uint8_t {
        MANUAL = 1,
        SYSTEM_TIME = 2,
        NTP_SYNC = 3,
    };

    int calcInteger(IntLogic INT_LOGIC, Channel CHANNEL_NUM);

    /// Centralized error handler for CLI functions
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

public:
    void openCommunicationCLI();

    [[nodiscard]] bool isResponsiveCLI();

    void setFwVerCLI();

    void detectFwVerCLI();

    void generatePulsesCLI();

    void setBaudRateCLI();

    void readBatchMeasurementsCLI();

    void setTimeConstantCLI();

    void resetCLI();

    ///
    /// Constructor
    /// Handle the NPET communication initialization in CLI mode.
    /// Have the user select the COM port if it can't be auto-detected.
    /// Detect the NPET firmware version automatically.
    explicit NPETCommCLI() {
        SPDLOG_INFO(COMM_INIT);
        Cli::echo(std::string(COMM_INIT), fg::blue, style::bold);
        openCommunicationCLI();
        detectFwVerCLI();
    } // end of constructor

    NPETCommCLI(const NPETCommCLI &) = delete;
    NPETCommCLI &operator=(const NPETCommCLI &) = delete;
    NPETCommCLI(NPETCommCLI &&) = delete;
    NPETCommCLI &operator=(NPETCommCLI &&) = delete;

    /// Destructor
    ~NPETCommCLI() {
        SPDLOG_INFO(COMM_CLOSE);
        Cli::echo(std::string(COMM_CLOSE), fg::blue, style::bold);
        Cli::echo("Baud rate will be reset to 115200 and COM port will be closed");
    } // end of destructor
};


#endif //NPET_COMM_CLI_H
