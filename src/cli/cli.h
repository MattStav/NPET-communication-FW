#ifndef CLI_H
#define CLI_H
#include <mutex>
#include <rang.hpp>
#include <string>
#include <vector>

using namespace rang;

///
/// Command Line Interface (CLI) class for user interaction.
class Cli {
public:
    ///
    /// Print a styled message to the console.
    /// @param msg Message to print
    /// @param END_LINE Whether to end the line after the message
    /// @param FG_COLOR Foreground color
    /// @param STYLE_TYPE Style type
    static void echo(
        const std::string &msg,
        fg FG_COLOR = fg::gray,
        style STYLE_TYPE = style::reset,
        bool END_LINE = true
    );

    ///
    /// Print an error message to the console.
    /// @param msg Message to print
    static void err(const std::string &msg);

    ///
    /// Show a value in CLI
    /// @param msg Message to print
    /// @param value Value to show
    static void showInt(const std::string &msg, const int &value);

    ///
    /// Show a value in CLI
    /// @param msg Message to print
    /// @param value Value to show
    static void showStr(const std::string &msg, const std::string &value);

    ///
    /// Ask for user confirmation with a yes/no question.
    /// @param question User confirm question
    /// @param default_yes Default to yes if True else False
    /// @return Bool value of user input
    [[nodiscard]] static bool confirm(const std::string &question, const bool &default_yes = false);

    ///
    /// Ask the user to press Enter to exit the program.
    static void confirmExit();

    ///
    /// Ask for user input with a question prompt. If the user input is empty, return the default value.
    /// @param question User prompt question
    /// @param default_value Default value if the user input is empty
    /// @return User input or default value
    [[nodiscard]] static std::string prompt(const std::string &question, const std::string &default_value = "");

    ///
    /// List a menu of options and ask the user to select one.
    /// @param title Menu title
    /// @param options Menu options
    /// @param END_LINE Whether to end the line after the menu
    /// @return Index of the selected option (1-based). Returns -2 for invalid input.
    [[nodiscard]] static int menu(
        const std::string &title,
        const std::vector<std::string> &options,
        bool END_LINE = true
    );
}; // end of cli class


///
/// CLI progress bar that shows the progress of a long-running operation.
/// The progress bar is updated by calling the update() method with the current progress value.
/// The progress bar is only redrawn if the percentage change since the last redraw exceeds a specified threshold, which can be set in the constructor.
struct ProgressBarConfig {
    int total{};
    int min_percent_change_to_redraw{1};
    int bar_width{50};
};

class ProgressBar {
public:
    ///
    /// Create a CLI Progress bar object which can be used to show process progress.
    /// @param CONFIG Progress bar configuration (total, redraw threshold, bar width)
    explicit ProgressBar(ProgressBarConfig CONFIG);

    ///
    /// Update the progress bar. Thread safe: may be called concurrently from multiple
    /// threads, with updates serialized so the bar is only ever redrawn once at a time.
    /// Whether the bar is redraw in CLI depends on change_trigger_ parameter.
    /// @param PROGRESS Current progress value (0 to total_)
    void update(int PROGRESS);

private:
    int total_;
    int change_trigger_;
    int bar_width_;
    int prev_bucket_ = -1;
    std::mutex mutex_;
};

#endif //CLI_H
