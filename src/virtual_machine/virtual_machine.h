#ifndef NPET_COMM_FW_VM_H
#define NPET_COMM_FW_VM_H
#include <chrono>

#include "meas_func.h"
#include "serial_machine.h"

struct VmConfig {
    int com_port{};
    int ch1_frequency{100};
    int corrupt_every{0};
    int ch1_delay_ns{0};
};


class VirtualMachine : public SerialMachine {
    // Frequency of measurement stream on channel 1
    int ch1_frequency_{};
    // Delay of channel 1's tick grid relative to channel 2's, see VmConfig::ch1_delay_ns
    std::chrono::nanoseconds ch1_delay_ns_{0};
    // Corrupt every n-th measurement
    int corrupt_every_{};
    // The time constant currently saved in NPET
    std::string time_const_;
    // The measurement counter
    uint8_t measurement_counter_ = 0;
    // Flag denoting whether NPET_FW valid measurement format is set
    bool correct_meas_format_set_ = false;
    // The time when vm was started
    const std::chrono::time_point<std::chrono::high_resolution_clock> START_TIME =
            std::chrono::high_resolution_clock::now();
    // Fixed per-launch timing offset (tens of us, in seconds), simulating a device's inherent, constant clock skew
    const __float128 TIMING_OFFSET = randomOffset();

    ///
    /// Generate a fixed, random timing offset for this VM instance, simulating a real device's
    /// small, constant clock skew that stays put for as long as it's powered on.
    /// @return A random offset expressed in seconds
    [[nodiscard]] static __float128 randomOffset();

    ///
    /// Process a received command and generate an appropriate response.
    /// @param command Command string received
    /// @return The response string to send back
    std::string getResponse(const std::string &command);

    ///
    /// Change the baud rate of the serial port for the virtual device.
    /// @param NEW_BAUD_RATE New baud rate to set
    void changeBaudRate(int NEW_BAUD_RATE);

    ///
    /// Simulate sending measurement data over the serial port.
    /// Measurements are fired on a fixed grid of times anchored to start_time (i.e. at start_time + k * period for
    /// integer k). This mirrors a real NPET device, whose measurement ticks are driven by a free-running internal clock.
    /// The first measurement is sent on the next upcoming grid tick.
    /// Concurrently listens for an incoming stop command ('c'), which the client sends (e.g. via NPET_comm::is_responsive)
    /// when the user cancels the measurement mid-stream (see meas_reader's Esc handling). If it arrives before all
    /// measurements have been sent, streaming stops early and a "c1" response is sent back, mirroring how a real NPET
    /// device acknowledges a stop request received while still streaming.
    /// @param num_str Number of measurements to send as a string
    /// @param PERIOD Fixed time between measurements (e.g. 1s, 250ms, 400us), anchored to start_time
    /// @param OFFSET Additional delay added to start_time before laying out the grid, letting a channel's
    /// ticks (and thus the measurement timestamps sent) lag behind another channel's by a fixed amount
    void sendMeasurements(const std::string &num_str, std::chrono::microseconds PERIOD,
                          std::chrono::nanoseconds OFFSET = std::chrono::nanoseconds(0));

protected:
    ///
    /// Get the current virtual machine run time
    /// @return The current run time of the virtual machine formatted as hh:mm:ss.
    [[nodiscard]] std::string getRunTime() const;

public:
    explicit VirtualMachine(const VmConfig CONFIG) : ch1_frequency_(CONFIG.ch1_frequency),
                                                     corrupt_every_(CONFIG.corrupt_every),
                                                     ch1_delay_ns_(std::chrono::nanoseconds(CONFIG.ch1_delay_ns)) {
    }

    ///
    /// Mock NPET device function that simulates the behavior of a real NPET device.
    /// Receives commands over a serial port and responds accordingly.
    void deviceLoop();
};


#endif //NPET_COMM_FW_VM_H
