#include <iostream>
#include <string>
#include <fstream>

#include "git_tag.h"
#include "framework/NPET_comm.h"
#include "framework/helper_func.h"
#include "framework/logging.h"
#include "cli/helper_func.h"
#include "virtual_machine/vm_main.h"

#include <CLI/CLI.hpp>
#include <spdlog/async.h>
#include <spdlog/spdlog.h>

#include "rang.hpp"


int main(const int argc, char *const*argv) {
    // Initialize logging thread pool first
    spdlog::init_thread_pool(8192, 1); // queue size, 1 background thread
    // The embedded license/manual text is UTF-8; switch the console over so it renders correctly
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    constexpr std::string_view APP_NAME = "NPET communication FW CLI";
    CLI::App app{std::string(APP_NAME)}; // CLI11 app object
    app.description("This program allows communication with the NPET device via command line interface.");
    app.set_version_flag("-v,--version", std::string(APP_NAME) + " (" + BUILD_CONFIG + ") " + GIT_TAG);
    const auto *const RUN = app.add_subcommand("run", "Run the app [default]");
    const auto *const MANUAL = app.add_subcommand("manual", "Show manual");
    const auto *const RESET = app.add_subcommand("reset", "Reset the NPET");
    auto *const VM = app.add_subcommand("virtual", "Run virtual machine NPET which can be used to test the FW");
    VM->set_help_flag("-h,--help", "Show help for the virtual command");
    int vm_ch1_frequency = 100;
    int vm_com_port{};
    VM->add_option("-f,--frequency", vm_ch1_frequency, "Data measurement frequency [Hz] on channel 1")
            ->check(CLI::Range(1, 2500));
    VM->add_option("--com-port", vm_com_port, "COM port number the virtual device connects to")
            ->required();
    const auto *const DATA_PROCESSOR = app.add_subcommand(
        "dp", "Run the NPET data processor, which needs to be installed separately");
    const auto *const LICENSE = app.add_subcommand("license", "Show license information");
    CLI11_PARSE(app, argc, argv);
    int exit_code = 1;
    if (*MANUAL) {
        exit_code = printManual();
    } else if (*RESET) {
        exit_code = resetNpetStandalone();
    } else if (*VM) {
        exit_code = launchVm({.com_port = vm_com_port, .ch1_frequency = vm_ch1_frequency});
    } else if (*DATA_PROCESSOR) {
        exit_code = launchDataProcessor();
    } else if (*LICENSE) {
        exit_code = printLicenseInformation();
    } else if (*RUN || app.get_subcommands().empty()) {
        exit_code = singleNPETMainMenu();
        Cli::confirmExit();
    }
    spdlog::shutdown(); // Ensure all logs are flushed before exiting
    return exit_code;
} // end of the main function
