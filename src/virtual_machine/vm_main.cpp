#include "vm_main.h"

#include <spdlog/spdlog.h>

#include "virtual_machine.h"


///
/// Main function for the mock NPET device virtual machine.
/// Launches a mock NPET device that communicates over a specified COM port.
/// Requires virtual com ports to work!!!
int launch_vm(const int com_port, const int ch1_frequency) {
    assert(com_port > 0);
    assert(ch1_frequency > 0);
    if (ch1_frequency > 2500) throw std::invalid_argument("Frequency must be less than or equal to 2500 Hz");
    SPDLOG_INFO("Mock NPET device virtual machine starting ...");
    auto vm = VirtualMachine(ch1_frequency);
    SPDLOG_INFO("User specified COM{} ...", com_port);
    vm.open_communication(com_port - 1, 115200);
    SPDLOG_INFO("Mock NPET device virtual machine COM port open");
    using period = std::chrono::high_resolution_clock::period;
    constexpr double tick_ns = static_cast<double>(period::num) * 1e9 / period::den;
    SPDLOG_INFO("Mock tick period: {} ns", tick_ns);
    SPDLOG_INFO("Mock is steady: {}", std::chrono::high_resolution_clock::is_steady);
    vm.device_loop();
    return 0;
} // end of launch_vm function
