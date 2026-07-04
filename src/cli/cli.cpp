#include "cli.h"

#include <iostream>
#include <string>
#include <vector>
#include <rang.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>  // enables formatting of vectors, arrays, etc.

using namespace std;
using namespace rang;

constexpr std::string_view MENU_INVALID_CHOICE = "Invalid choice {}";

///
/// Print a styled message to the console.
/// @param msg Message to print
/// @param fg_color Foreground color
/// @param style_type Style type
/// @param end_line Whether to end the line after the message
void cli::echo(const string &msg, const fg fg_color, const style style_type, const bool end_line) {
    SPDLOG_DEBUG("CLI -> {}", msg);
    cout << style_type << fg_color << msg << style::reset;
    end_line ? cout << endl : cout << "";
} // end of echo function


///
/// Print an error message to the console.
/// @param msg Message to print
void cli::err(const string &msg) {
    SPDLOG_ERROR("CLI ERROR -> {}", msg);
    cout << style::bold << style::reversed << fg::red << msg << " " << style::reset << endl;
} // end of echo function


///
/// Show a value in CLI
/// @param msg Message to print
/// @param value Value to show
void cli::show_int(const string &msg, const int &value) {
    SPDLOG_DEBUG("CLI SHOW -> {}; Value: {}", msg, value);
    cout << msg << ": " << style::italic << bg::blue << " " << value << " " << style::reset << endl;
} // end of show_int function


///
/// Show a value in CLI
/// @param msg Message to print
/// @param value Value to show
void cli::show_str(const string &msg, const string &value) {
    SPDLOG_DEBUG("CLI SHOW -> {}; Value: {}", msg, value);
    cout << msg << ": " << style::italic << bg::blue << " " << value << " " << style::reset << endl;
} // end of show_str function


///
/// Ask for user confirmation with a yes/no question.
/// @param question User confirm question
/// @param default_yes Default to yes if True else False
/// @return Bool value of user input
bool cli::confirm(const string &question, const bool &default_yes) {
    SPDLOG_DEBUG("CLI CONFIRM -> {}; default? {}", question, default_yes);
    string answer;
    bool ret = default_yes;

    const string choices = default_yes ? " [Y/n] " : " [y/N] ";
    cout << fg::cyan << question << fg::reset << choices;
    getline(cin, answer);
    // Normalize input
    ranges::transform(answer, answer.begin(), ::tolower);
    if (!answer.empty()) ret = answer == "y" || answer == "yes" || answer == "1";
    SPDLOG_DEBUG("CLI CONFIRM -> User input: {}; interpreted as: {}", answer, ret);
    return ret;
} // end of confirm function


///
/// Ask the user to press Enter to exit the program.
void cli::confirm_exit() {
    SPDLOG_DEBUG("CLI CONFIRM EXIT -> Asking user to press Enter to exit the program");
    cout << "Press " << style::blink << fg::red << "Enter" << style::reset << " to exit the program." << endl;
    string dummy;
    getline(cin, dummy);
    SPDLOG_DEBUG("CLI CONFIRM EXIT -> Confirmed");
} // end of confirm_exit function


///
/// Ask for user input with a question prompt. If the user input is empty, return the default value.
/// @param question User prompt question
/// @param default_value Default value if the user input is empty
/// @return User input or default value
string cli::prompt(const string &question, const string &default_value) {
    SPDLOG_DEBUG("CLI PROMPT -> {}; default: {}", question, default_value);
    string input;

    cout << fg::cyan << question;
    if (!default_value.empty()) {
        cout << fg::gray << " [" << default_value << "]";
    }
    cout << fg::cyan << ": " << fg::reset;
    getline(cin, input);
    std::string ret = input.empty() ? default_value : input;
    SPDLOG_DEBUG("CLI PROMPT -> User input: {}", ret);
    return ret;
} // end of prompt function


///
/// List a menu of options and ask the user to select one.
/// @param title Menu title
/// @param options Menu options
/// @param end_line Whether to end the line after the menu
/// @return Index of the selected option (1-based). Returns -2 for invalid input.
int cli::menu(const string &title, const vector<string> &options, const bool end_line) {
    string user_choice{};
    int choice_int{};
    SPDLOG_DEBUG("CLI MENU -> {}; options: {}", title, options);
    // Display the menu options
    cout << style::underline << "--- " << title << " ---" << style::reset << endl;
    for (size_t i = 0; i < options.size(); ++i) {
        cout << i + 1 << ". " << options[i] << endl;
    }
    user_choice = prompt("Select a number corresponding to an option");
    try {
        choice_int = stoi(user_choice);
        if (choice_int < 1 || choice_int > static_cast<int>(options.size()))
            throw invalid_argument("Choice out of range");
    } catch (const invalid_argument &) {
        SPDLOG_ERROR(MENU_INVALID_CHOICE, choice_int);
        err(std::format(MENU_INVALID_CHOICE, choice_int));
        std::cout << std::endl;
        return -2;
    }
    SPDLOG_DEBUG("CLI MENU -> User selected: {}", user_choice);
    if (end_line) std::cout << std::endl;
    return choice_int;
} // end of menu function


///
/// Draw a progress bar in CLI, redraws a previous line.
/// @param percentage Current progress percentage (0-100)
/// @param bar_width The bar width in CLI
void draw_progress_bar(const int percentage, const int bar_width) {
    const int pos = percentage * bar_width / 100;
    std::cout << "\033[1A\r["
            << std::string(pos, '=')
            << (pos < bar_width ? ">" : "")
            << std::string(std::max(0, bar_width - pos - 1), ' ')
            << "] "
            << percentage << "%"
            << std::flush
            << std::endl;
}


///
/// Create a CLI Progress bar object which can be used to show process progress.
/// @param total Total value corresponding to 100% progress
/// @param min_percent_change_to_redraw Minimum percentage change required to redraw the progress bar
/// @param bar_width The bar width in CLI
ProgressBar::ProgressBar(const int total, const int min_percent_change_to_redraw, const int bar_width)
    : total_(total), change_trigger_(min_percent_change_to_redraw), bar_width_(bar_width) {
    assert(total_ > 0);
    assert(change_trigger_ > 0);
    std::cout << std::endl;
    draw_progress_bar(0, bar_width_);
}


///
/// Update the progress bar.
/// Whether the bar is redraw in CLI depends on change_trigger_ parameter.
/// @param progress The latest progress value
void ProgressBar::update(const int progress) {
    const float ratio = static_cast<float>(progress) / static_cast<float>(total_);
    const int percentage = static_cast<int>(ratio * 100.0f);
    if (percentage != prev_bucket_ && percentage / change_trigger_ != prev_bucket_) {
        draw_progress_bar(percentage, bar_width_);
    }
    prev_bucket_ = percentage / change_trigger_;;
}
