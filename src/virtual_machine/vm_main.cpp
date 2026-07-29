#include "vm_main.h"

#include <spdlog/spdlog.h>

#include "virtual_machine.h"


///
/// Main function for the mock NPET device virtual machine.
/// Launches a mock NPET device that communicates over a specified COM port.
/// Requires virtual com ports to work!!!
/// @param CONFIG Virtual machine configurations
int launchVm(const VmConfig CONFIG) {
    assert(CONFIG.com_port > 0);
    assert(CONFIG.ch1_frequency > 0);
    if (CONFIG.ch1_frequency > 2500) {
        throw std::invalid_argument("Frequency must be less than or equal to 2500 Hz");
    }
    SPDLOG_INFO("Mock NPET device virtual machine starting ...");
    auto vm = VirtualMachine(CONFIG.ch1_frequency);
    SPDLOG_INFO("User specified COM{} ...", CONFIG.com_port);
    vm.openCommunication(CONFIG.com_port - 1, 115200);
    SPDLOG_INFO("Mock NPET device virtual machine COM port open");
    using period = std::chrono::high_resolution_clock::period;
    constexpr double TICK_NS = static_cast<double>(period::num) * 1e9 / period::den;
    SPDLOG_INFO("Mock tick period: {} ns", TICK_NS);
    SPDLOG_INFO("Mock is steady: {}", std::chrono::high_resolution_clock::is_steady);
    vm.deviceLoop();
    return 0;
} // end of launchVm function
