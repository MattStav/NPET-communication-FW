#ifndef NPET_COMM_CLI_H
#define NPET_COMM_CLI_H
#include <cstdint>
#include <utility>

#include "NPET_comm.h"
#include "cli.h"
#include "helper_func.h"
#include "safe_exec.h"

constexpr std::string_view COMM_INIT = "Initializing NPET communication framework";
constexpr std::string_view COMM_CLOSE = "Closing NPET communication framework";


class NPETCommCLI : public NPETComm {
    Measurement rawTimeConstant();

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
        Cli::echo("Baud rate will be reset to " + std::to_string(DEFAULT_BAUD_RATE) + " and COM port will be closed");
    } // end of destructor
};


#endif //NPET_COMM_CLI_H
