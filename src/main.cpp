#include "cli/arg_parser.h"
#include "cli/helper_func.h"
#include "cli/cli.h"
#include "virtual_machine/vm_main.h"
#include "workflows.h"

#include <spdlog/async.h>
#include <spdlog/spdlog.h>


int main(const int argc, char *const*argv) {
    // Initialize logging thread pool first
    spdlog::init_thread_pool(8192, 1); // queue size, 1 background thread
    // The embedded license/manual text is UTF-8; switch the console over so it renders correctly
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    const auto [early_exit_code, command, vm_config] = parseArgs(argc, argv);
    if (early_exit_code) {
        spdlog::shutdown();
        return *early_exit_code;
    }
    int exit_code = 1;
    switch (command) {
        case AppCommand::Manual:
            spdlog::set_level(spdlog::level::off);
            exit_code = printManual();
            break;
        case AppCommand::Reset:
            spdlog::set_level(spdlog::level::off);
            exit_code = resetNpetStandalone();
            break;
        case AppCommand::Virtual:
            spdlog::set_level(spdlog::level::off);
            exit_code = launchVm(vm_config);
            break;
        case AppCommand::DataProcessor:
            spdlog::set_level(spdlog::level::off);
            exit_code = launchDataProcessor();
            break;
        case AppCommand::License:
            spdlog::set_level(spdlog::level::off);
            exit_code = printLicenseInformation();
            break;
        case AppCommand::Single:
            exit_code = singleNPETMainMenu();
            Cli::confirmExit();
            break;
        case AppCommand::Dual:
            exit_code = dualNPETMainMenu();
            Cli::confirmExit();
            break;
        case AppCommand::None:
            break;
    }
    spdlog::shutdown(); // Ensure all logs are flushed before exiting
    return exit_code;
} // end of the main function
