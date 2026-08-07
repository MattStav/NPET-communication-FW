#ifndef NPET_COMM_FW_NPET_DUAL_CLI_H
#define NPET_COMM_FW_NPET_DUAL_CLI_H
#include <spdlog/spdlog.h>

#include "cli.h"
#include "helper_func.h"
#include "NPET_dual.h"

constexpr std::string_view COMM_DUAL_INIT = "Initializing NPETDual communication framework";
constexpr std::string_view COMM_DUAL_CLOSE = "Closing NPETDual communication framework";


class NPETDualCLI : public NPETDual {
public:
    void openCommunicationCLI();

    [[nodiscard]] bool bothResponsiveCLI();

    void detectFwVerCLI();

    void readBatchMeasurementsCLI();

    void setTimeConstantCLI();

    void resetCLI();

    ///
    /// Constructor
    /// Handle the NPETDual communication initialization in CLI mode.
    /// Have the user select the COM ports and detect the NPETs firmware version automatically.
    explicit NPETDualCLI() {
        SPDLOG_INFO(COMM_DUAL_INIT);
        Cli::echo(std::string(COMM_DUAL_INIT), fg::blue, style::bold);
        openCommunicationCLI();
        detectFwVerCLI();
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
