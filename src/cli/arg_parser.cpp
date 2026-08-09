#include "arg_parser.h"

#include <string>

#include "git_tag.h"

#include <CLI/CLI.hpp>

#include "helper_func.h"

ParsedArgs parseArgs(const int argc, char *const *argv) {
    CLI::App app{std::string(APP_NAME)}; // CLI11 app object
    app.description("This program allows communication with the NPET device via command line interface.");
    app.set_version_flag("-v,--version", std::string(APP_NAME) + " (" + BUILD_CONFIG + ") " + GIT_TAG);
    const auto *const SINGLE = app.add_subcommand("single", "Run the app for single NPET [default]");
    const auto *const DUAL = app.add_subcommand("dual", "Run the app for dual NPET");
    const auto *const MANUAL = app.add_subcommand("manual", "Show manual");
    const auto *const RESET = app.add_subcommand("reset", "Reset connected NPET to default settings");
    auto *const VM = app.add_subcommand("virtual", "Run virtual machine NPET which can be used to test the FW");
    VM->set_help_flag("-h,--help", "Show help for the virtual command");
    VmConfig vm_config{.ch1_frequency = 100};
    VM->add_option("-f,--frequency", vm_config.ch1_frequency, "Data measurement frequency [Hz] on channel 1")
            ->check(CLI::Range(1, 2500));
    VM->add_option("--com-port", vm_config.com_port, "COM port number the virtual device connects to")
            ->required();
    VM->add_option("--corrupt-every", vm_config.corrupt_every, "Corrupt every N measurements")
            ->check(CLI::Range(0, 2500));
    VM->add_option("--offset", vm_config.ch1_delay_ns,
                   "Delay of channel 1 relative to channel 2 (PPS, fixed 1 Hz) [ns]")
            ->check(CLI::Range(0, 999));
    const auto *const DATA_PROCESSOR = app.add_subcommand(
        "dp", "Run the NPET data processor, which needs to be installed separately");
    const auto *const LICENSE = app.add_subcommand("license", "Show license information");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return {.early_exit_code = app.exit(e)};
    }

    ParsedArgs result{.vm_config = vm_config};
    if (*MANUAL) {
        result.command = AppCommand::Manual;
    } else if (*RESET) {
        result.command = AppCommand::Reset;
    } else if (*VM) {
        result.command = AppCommand::Virtual;
    } else if (*DATA_PROCESSOR) {
        result.command = AppCommand::DataProcessor;
    } else if (*LICENSE) {
        result.command = AppCommand::License;
    } else if (*SINGLE || app.get_subcommands().empty()) {
        result.command = AppCommand::Single;
    } else if (*DUAL) {
        result.command = AppCommand::Dual;
    }
    return result;
}
