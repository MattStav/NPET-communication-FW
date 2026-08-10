#include "vm_main.h"

#include <spdlog/spdlog.h>


int launchVm(const VmConfig CONFIG) {
    assert(CONFIG.com_port > 0);
    assert(CONFIG.ch1_frequency > 0);
    if (CONFIG.ch1_frequency > 2500) {
        throw std::invalid_argument("Frequency must be less than or equal to 2500 Hz");
    }
    SPDLOG_INFO("Mock NPET device virtual machine starting ...");
    auto vm = VirtualMachine(CONFIG);
    SPDLOG_INFO("User specified COM{} ...", CONFIG.com_port);
    vm.ser.openCommunication(CONFIG.com_port, DEFAULT_BAUD_RATE);
    SPDLOG_INFO("Mock NPET device virtual machine COM port open");
    SPDLOG_INFO("User specified CH1 frequency: {} Hz", CONFIG.ch1_frequency);
    using period = std::chrono::high_resolution_clock::period;
    constexpr double TICK_NS = static_cast<double>(period::num) * 1e9 / period::den;
    SPDLOG_INFO("Virtual clock tick period: {} ns", TICK_NS);
    SPDLOG_INFO("Virtual clock is steady: {}", std::chrono::high_resolution_clock::is_steady);
    vm.deviceLoop();
    vm.ser.closeCommunication();
    SPDLOG_INFO("Mock NPET device virtual machine stopped");
    return 0;
} // end of launchVm function
