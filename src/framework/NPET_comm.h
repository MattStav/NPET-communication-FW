#ifndef NPET_COMMUNICATOR_H
#define NPET_COMMUNICATOR_H
#include <functional>
#include <type_traits>
#include <spdlog/spdlog.h>

#include "meas_reader.h"
#include "meas_func.h"
#include "fw_version.h"
#include "serial_machine.h"


class NPETComm : public SerialMachine {
public:
    // Internal NPET firmware version
    FWVersion fw_version{};

    // Check if NPET is responsive
    [[nodiscard]] bool isResponsive(bool END_STREAM = false);

    // Functions to select NPET firmware version and save it into fw_version attribute
    void setFWVer(FWVersion NEW_FW_VERSION);

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
        .save_path = std::nullopt,
        .channel = Channel::CH1,
    });

    Measurement readSingleMeasurement(Channel CHANNEL);

    std::string readSingleMeasurementRaw(Channel CHANNEL);

    // Time correction constant handling on NPET
    [[nodiscard]] bool exportTimeConstant(const Measurement &constant);

    [[nodiscard]] bool exportTimeConstantRaw(const std::string &constant_raw = "");

    [[nodiscard]] bool clearTimeConstant();

    Measurement importTimeConstant();

    std::string importTimeConstantRaw();

    std::string getStatus();

    // Progress is optionally reported by calling PROGRESS->update(int) with the number of measurements taken so far
    template<typename ProgressT = void>
    std::optional<__float128> getAverageFraction(const int AVER_NUM = 16, Channel CHANNEL_NUM = Channel::CH2,
                                                 ProgressT *PROGRESS = nullptr) {
        if constexpr (std::is_void_v<ProgressT>) {
            return getAverageFractionImpl(AVER_NUM, CHANNEL_NUM, nullptr);
        } else {
            return getAverageFractionImpl(AVER_NUM, CHANNEL_NUM, [PROGRESS](const int PROGRESS_VAL) {
                PROGRESS->update(PROGRESS_VAL);
            });
        }
    }

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

private:
    std::optional<__float128> getAverageFractionImpl(int AVER_NUM, Channel CHANNEL_NUM,
                                                     const std::function<void(int)> &PROGRESS_FN);
};


///
/// Set measurement data format to binary on an NPET
/// @param npet NPETComm instance to set the measured data format
inline void setMeasuredDataFormatToBinary(NPETComm &npet) {
    SPDLOG_DEBUG("Setting measured data format to binary for NPET");
    if (!npet.setMeasuredDataFormat(0)) {
        SPDLOG_ERROR(DATA_FORMAT_ERR);
        throw std::runtime_error(std::string(DATA_FORMAT_ERR));
    }
}


#endif //NPET_COMMUNICATOR_H
