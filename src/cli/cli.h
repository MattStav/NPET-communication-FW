#ifndef CLI_H
#define CLI_H
#include <rang.hpp>
#include <string>
#include <vector>

using namespace rang;

///
/// Command Line Interface (CLI) class for user interaction.
class Cli {
public:
    static void echo(
        const std::string &msg,
        fg FG_COLOR = fg::gray,
        style STYLE_TYPE = style::reset,
        bool END_LINE = true
    );

    static void err(const std::string &msg);

    static void showInt(const std::string &msg, const int &value);

    static void showStr(const std::string &msg, const std::string &value);

    [[nodiscard]] static bool confirm(const std::string &question, const bool &default_yes = false);

    static void confirmExit();

    [[nodiscard]] static std::string prompt(const std::string &question, const std::string &default_value = "");

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
class ProgressBar {
public:
    explicit ProgressBar(int TOTAL, int MIN_PERCENT_CHANGE_TO_REDRAW = 1, int BAR_WIDTH = 50);

    void update(int PROGRESS);

private:
    int total_;
    int change_trigger_;
    int bar_width_;
    int prev_bucket_ = -1;
};

#endif //CLI_H
