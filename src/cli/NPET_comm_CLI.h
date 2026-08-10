#ifndef NPET_COMM_CLI_H
#define NPET_COMM_CLI_H
#include <cstdint>
#include <utility>

#include "NPET_comm.h"
#include "cli.h"
#include "safe_exec.h"

constexpr std::string_view COMM_INIT = "Initializing NPET communication framework";
constexpr std::string_view COMM_CLOSE = "Closing NPET communication framework";


class NPETCommCLI : public NPETComm {
public:
    ///
    /// Open serial communication with NPET
    /// Initial detection of the COM port is attempted first, followed by user prompt if detection fails.
    void openCommunicationCLI();

    ///
    /// Checks if the NPET device is connected to the specified COM port and responsive.
    /// Several attempts are made with a delay between each attempt.
    /// Prints the status to the CLI.
    /// @return True if NPET is connected and responsive, otherwise false.
    [[nodiscard]] bool isResponsiveCLI();

    ///
    /// Ask the user to select what version of FW the connected NPET device is running.
    /// Firmware version is saved into the fw_version attribute.
    void setFwVerCLI();

    ///
    /// CLI wrapper for detect_FW_ver function.
    void detectFwVerCLI();

    ///
    /// CLI wrapper for the generate_pulses function.
    /// Asks the user for the number of pulses to generate and the frequency.
    void generatePulsesCLI();

    ///
    /// ClI wrapper for the set_baud_rate function.
    void setBaudRateCLI();

    ///
    /// CLI wrapper for the read_measurements function.
    /// Asks the user for the number of measurements, channel to read from, display, and save options
    void readBatchMeasurementsCLI();

    ///
    /// Set the time correction constant on the NPET device.
    /// User can choose between raw format input, time format input, or clearing the constant.
    /// Either way, the time correction constant saved in the NPET and in the program is always the same.
    /// In the end, the constant is exported to the NPET and sample measurements are read to show the effect.
    void setTimeConstantCLI();

    ///
    /// Reset NPET into default settings.
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
