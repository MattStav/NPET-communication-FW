#ifndef NPET_COMM_FW_NPET_DUAL_CLI_H
#define NPET_COMM_FW_NPET_DUAL_CLI_H
#include <spdlog/spdlog.h>

#include "cli.h"
#include "helper_func.h"
#include "NPET_dual.h"
#include "safe_exec.h"

constexpr std::string_view COMM_DUAL_INIT = "Initializing NPETDual communication framework";
constexpr std::string_view COMM_DUAL_CLOSE = "Closing NPETDual communication framework";


class NPETDualCLI : public NPETDual {
public:
    ///
    /// Open serial communication with both NPETs.
    void openCommunicationCLI();

    ///
    /// Checks if both NPET devices are connected to the specified COM ports and responsive.
    /// Several attempts are made with a delay between each attempt.
    /// Prints the status to the CLI.
    /// @return True if both NPETs are connected and responsive, otherwise false.
    [[nodiscard]] bool bothResponsiveCLI();

    ///
    /// CLI wrapper for the reading batch measurements from both NPETs.
    /// Asks the user for the number of measurements, channel to read from, display, and save options.
    void readBatchMeasurementsCLI();

    ///
    /// Switch the NPET START/STOP designation.
    void switchStartStopCLI();

    ///
    /// CLI wrapper for setting baud rate on both NPETs
    void setBaudRateCLI();

    ///
    /// Synchronize the two NPETs.
    void syncNPETsCLI();

    ///
    /// CLI wrapper to set the NPET firmware version for both NPETs.
    void setFwVerCLI();

    ///
    /// Reset both NPETs into default settings.
    void resetCLI();

    ///
    /// Constructor
    /// Handle the NPETDual communication initialization in CLI mode.
    /// Have the user select the COM ports and detect the NPETs firmware version automatically.
    explicit NPETDualCLI() {
        SPDLOG_INFO(COMM_DUAL_INIT);
        Cli::echo(std::string(COMM_DUAL_INIT), fg::blue, style::bold);
        openCommunicationCLI();
    } // end of constructor

    ~NPETDualCLI() {
        SPDLOG_INFO(COMM_DUAL_CLOSE);
        Cli::echo(std::string(COMM_DUAL_CLOSE), fg::blue, style::bold);
        Cli::echo("Baud rate will be reset to " + std::to_string(DEFAULT_BAUD_RATE) + " and COM ports will be closed");
    } // end of destructor

    NPETDualCLI(const NPETDualCLI &) = delete;

    NPETDualCLI &operator=(const NPETDualCLI &) = delete;

    NPETDualCLI(NPETDualCLI &&) = delete;

    NPETDualCLI &operator=(NPETDualCLI &&) = delete;
};


#endif //NPET_COMM_FW_NPET_DUAL_CLI_H
