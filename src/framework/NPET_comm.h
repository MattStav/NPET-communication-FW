#ifndef NPET_COMMUNICATOR_H
#define NPET_COMMUNICATOR_H
#include <spdlog/spdlog.h>

#include "meas_reader.h"
#include "meas_func.h"
#include "fw_version.h"
#include "serial_machine.h"

static constexpr std::size_t MEASUREMENT_PACKET_SIZE = 13;
static constexpr int INFINITE_OPERATION = 9999;

class NPETComm : public SerialMachine {
public:
    // Internal NPET firmware version
    FWVersion fw_version{};

    // Check if NPET is responsive
    [[nodiscard]] bool isResponsive(bool END_STREAM = false);

    // Functions to select NPET firmware version and save it into fw_version attribute
    void setFWVer(int NEW_FW_VERSION);

    void detectFWVer();

    // Set the pulse generation frequency on the NPET device
    [[nodiscard]] bool setFrequency(int NEW_FREQUENCY = 100);

    // Generate pulses from the NPET device
    [[nodiscard]] bool generatePulses(int NUM_OF_PULSES = 0);

    // Set the baud rate on the NPET device
    [[nodiscard]] bool setBaudRate(int NEW_BAUD_RATE = 115200);

    // Set the way NPET sends the measured data, 0 = binary, 1 = ASCII
    [[nodiscard]] bool setMeasuredDataFormat(int FORMAT);

    // Read measurements from NPET
    void readBatchMeasurements(const MeasContext &meas_set = MeasContext{
        .num_of_meas = 5,
        .monitor_fn = nullptr,
        .save_dir = std::nullopt,
        .channel = 1,
    });

    Measurement readSingleMeasurement(int CHANNEL);

    std::string readSingleMeasurementRaw(int CHANNEL);

    // Time correction constant handling on NPET
    [[nodiscard]] bool exportTimeConstant(const Measurement &constant);

    [[nodiscard]] bool exportTimeConstantRaw(const std::string &constant_raw = "");

    [[nodiscard]] bool clearTimeConstant();

    Measurement importTimeConstant();

    std::string importTimeConstantRaw();

    std::string getStatus();

    NPETComm() = default;
    NPETComm(const NPETComm &) = delete;
    NPETComm &operator=(const NPETComm &) = delete;
    NPETComm(NPETComm &&) = delete;
    NPETComm &operator=(NPETComm &&) = delete;

    // Destructor
    ~NPETComm() {
        SPDLOG_DEBUG("NPET comm destructor called, closing communication and resetting baud rate if possible");
        // Reset to default baud rate
        if (isOpen()) {
            if (isResponsive()) {
                (void) setBaudRate(115200); // Ignore return value
            }
            closeCommunication();
        }
    } // end of destructor
};


#endif //NPET_COMMUNICATOR_H
