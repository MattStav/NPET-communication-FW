#ifndef NPET_COMMUNICATOR_H
#define NPET_COMMUNICATOR_H
#include <functional>
#include <type_traits>
#include <spdlog/spdlog.h>

#include "meas_reader.h"
#include "meas_func.h"
#include "fw_version.h"
#include "serial.h"


class NPETComm {
    // Internal NPET firmware version
    FWVersion fw_version_{};

public:
    // Serial connection
    Serial ser{};

    ///
    /// Checks if the NPET device is connected and responsive.
    /// @return: True, indicating the device is responsive. Otherwise, return false.
    [[nodiscard]] bool isResponsive(bool END_STREAM = false);

    ///
    /// Set the firmware version.
    /// Version is saved into the fw_version attribute.
    /// @param NEW_FW_VERSION New firmware version to set
    void setFWVer(FWVersion NEW_FW_VERSION);

    ///
    /// @return Current NPET internal FW version.
    FWVersion getFWVer() const { return fw_version_; }

    ///
    /// Automatically detect the NPET firmware version by querying the device.
    /// The firmware version is saved into the fw_version attribute.
    void detectFWVer();

    ///
    /// Set the pulse generation frequency on the NPET device.
    /// The Default frequency on NPET startup is 100 Hz.
    /// Save the new frequency into the frequency attribute.
    /// @param NEW_FREQUENCY New pulse generation frequency in Hz
    /// @return True if the frequency was successfully set, otherwise false
    [[nodiscard]] bool setFrequency(int NEW_FREQUENCY = 100);

    ///
    /// Command the NPET device to generate a specified number of pulses.
    /// @param NUM_OF_PULSES Number of pulses to generate, -1 for infinite
    /// @return True if the command was successful, otherwise false
    [[nodiscard]] bool generatePulses(int NUM_OF_PULSES = 0);

    ///
    /// Set the port baud rate.
    /// The default baud rate on NPET startup is 115_200.
    /// This operation cannot use the exchange_comm framework, making it very brittle.
    /// DO NOT DISCONNECT THE DEVICE WHILE CHANGING THE BAUD RATE.
    /// @param NEW_BAUD_RATE New baud rate to set
    /// @return True if the baud rate was successfully changed, otherwise false
    [[nodiscard]] bool setBaudRate(int NEW_BAUD_RATE = 115200);

    ///
    /// Set the measured data format on the NPET device.
    /// @param FORMAT Measured data format. 0 for binary, 1 for ASCII
    /// @return True if the format was successfully set, otherwise false
    [[nodiscard]] bool setMeasuredDataFormat(int FORMAT);

    ///
    /// Use the measurement_reader object to read measurements from the NPET device.
    /// Measurements are read in binary format.
    /// Windows sleep is disabled while the measurements are being read.
    /// @param meas_set Measurement context struct
    /// /// Contains the number of measurements, display and save flags, and channel number
    void readBatchMeasurements(const MeasContext &meas_set = MeasContext{
        .num_of_meas = 5,
        .monitor_fn = nullptr,
        .save_path = std::nullopt,
        .channel = Channel::CH1,
    });

    ///
    /// Read a single measurement from the specified channel.
    /// @param CHANNEL Channel to read from (1 or 2)
    /// @return Single measurement from the specified channel
    Measurement readSingleMeasurement(Channel CHANNEL);

    ///
    /// Read a single measurement from the specified channel in raw string format, without termination.
    /// @param CHANNEL Channel to read from (1 or 2)
    /// @return Single measurement from the specified channel in raw string format, without termination.
    /// /// Empty string if no measurement was read.
    std::string readSingleMeasurementRaw(Channel CHANNEL);

    ///
    /// Export the time constant to the NPET device.
    /// @param constant Time constant in measurement format
    /// @return True if the time constant was successfully exported, otherwise false.
    [[nodiscard]] bool exportTimeConstant(const Measurement &constant);

    ///
    /// Export the time constant to the NPET device.
    /// @param constant_raw Time constant in string format, without termination!
    /// Can be a maximum of 28 characters long.
    /// @return True if the time constant was successfully exported, otherwise false.
    [[nodiscard]] bool exportTimeConstantRaw(const std::string &constant_raw = "");

    ///
    /// Clear the time constant saved in the NPET devic.
    /// @return True if the time constant was successfully cleared on the NPET, otherwise false.
    [[nodiscard]] bool clearTimeConstant();

    ///
    /// Import the time correction constant from the NPET device.
    /// This should be the only way to get the time constant within the program!!
    /// Ensuring that the value saved in NPET and in the program are always the same.
    /// @return Measurement object containing the time constant.
    Measurement importTimeConstant();

    ///
    /// Import the time correction constant from the NPET device in raw string format, without termination.
    /// @return Time constant imported from the NPET device in raw string format, without termination.
    std::string importTimeConstantRaw();

    ///
    /// Get the status of the NPET device.
    /// This command is only of use when NPET is set in measurement streaming mode, but it is NOT receiving any data.
    /// @return Current status of the NPET device, check docu for more details.
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
        if (ser.isOpen()) {
            if (isResponsive()) {
                (void) setBaudRate(DEFAULT_BAUD_RATE); // Ignore return value
            }
            ser.closeCommunication();
        }
    } // end of destructor

    ///
    /// Get the time difference between PPS channel and system clock.
    /// The NPET should have synchronized fracp measurements before this is carried out.
    /// @param PPS_CHANNEL The channel that PPS signal is connected to
    /// @param clock_seconds Optional clock seconds, if not supplied then the system time is used
    /// @return The time difference in seconds between the PPS channel and current system time
    int getClockTimeDiff(Channel PPS_CHANNEL, std::optional<int> clock_seconds = std::nullopt);

private:
    ///
    /// Get the average fractional part of the measurements.
    /// @param AVER_NUM Number of measurements to average
    /// @param CHANNEL_NUM Channel number to read the measurements from (1 or 2)
    /// @param PROGRESS_FN Optional progress function to report the number of measurements taken so far
    /// @return The average fractional part of the measurements
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
