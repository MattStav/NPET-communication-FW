#ifndef NPET_COMM_FW_ARG_PARSER_H
#define NPET_COMM_FW_ARG_PARSER_H

#include <optional>

#include "virtual_machine.h"

///
/// The command selected on the CLI, dispatching which workflow main() should run.
/// None means no subcommand matched (should not normally occur; parseArgs() defaults to Single).
enum class AppCommand {
    None,
    Single,
    Dual,
    Manual,
    Reset,
    Virtual,
    DataProcessor,
    License,
};

///
/// Result of parsing the process' command line arguments.
struct ParsedArgs {
    // Set when CLI11 already handled the invocation (e.g. --help/--version or a parse error);
    // the caller should exit immediately with this code instead of dispatching a command.
    std::optional<int> early_exit_code;
    AppCommand command{AppCommand::None};
    VmConfig vm_config{};
};

///
/// Build the CLI11 app, parse argc/argv against it, and translate the result into a ParsedArgs.
/// @param argc Argument count, as passed to main()
/// @param argv Argument vector, as passed to main()
/// @return The selected command and its options, or an early_exit_code if CLI11 already handled the invocation
ParsedArgs parseArgs(int argc, char *const *argv);

#endif //NPET_COMM_FW_ARG_PARSER_H
