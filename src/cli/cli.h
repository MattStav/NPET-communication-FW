#ifndef CLI_H
#define CLI_H
#include <rang.hpp>
#include <string>
#include <vector>

using namespace rang;

///
/// Command Line Interface (CLI) class for user interaction.
class cli {
public:
    static void echo(
        const std::string &msg,
        fg fg_color = fg::gray,
        style style_type = style::reset,
        bool end_line = true
    );

    static void err(const std::string &msg);

    static void show_int(const std::string &msg, const int &value);

    static void show_str(const std::string &msg, const std::string &value);

    [[nodiscard]] static bool confirm(const std::string &question, const bool &default_yes = false);

    static void confirm_exit();

    [[nodiscard]] static std::string prompt(const std::string &question, const std::string &default_value = "");

    [[nodiscard]] static int menu(
        const std::string &title,
        const std::vector<std::string> &options,
        bool end_line = true
    );
}; // end of cli class


///
/// CLI progress bar that shows the progress of a long-running operation.
/// The progress bar is updated by calling the update() method with the current progress value.
/// The progress bar is only redrawn if the percentage change since the last redraw exceeds a specified threshold, which can be set in the constructor.
class ProgressBar {
public:
    explicit ProgressBar(int total, int min_percent_change_to_redraw = 1, int bar_width = 50);

    void update(int progress);

private:
    int total_;
    int change_trigger_;
    int bar_width_;
    int prev_bucket_ = -1;
};

#endif //CLI_H
