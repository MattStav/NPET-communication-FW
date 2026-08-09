#include "cli.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <rang.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>  // enables formatting of vectors, arrays, etc.

using namespace std;
using namespace rang;

constexpr std::string_view MENU_INVALID_CHOICE = "Invalid choice {}";

void Cli::echo(const string &msg, const fg FG_COLOR, const style STYLE_TYPE, const bool END_LINE) {
    SPDLOG_DEBUG("CLI -> {}", msg);
    cout << STYLE_TYPE << FG_COLOR << msg << style::reset;
    END_LINE ? cout << '\n' : cout << "";
} // end of echo function


void Cli::err(const string &msg) {
    SPDLOG_ERROR("CLI ERROR -> {}", msg);
    cout << style::bold << style::reversed << fg::red << msg << " " << style::reset << '\n';
} // end of echo function


void Cli::showInt(const string &msg, const int &value) {
    SPDLOG_DEBUG("CLI SHOW -> {}; Value: {}", msg, value);
    cout << msg << ": " << style::italic << bg::blue << " " << value << " " << style::reset << '\n';
} // end of show_int function


void Cli::showStr(const string &msg, const string &value) {
    SPDLOG_DEBUG("CLI SHOW -> {}; Value: {}", msg, value);
    cout << msg << ": " << style::italic << bg::blue << " " << value << " " << style::reset << '\n';
} // end of show_str function


bool Cli::confirm(const string &question, const bool &default_yes) {
    SPDLOG_DEBUG("CLI CONFIRM -> {}; default? {}", question, default_yes);
    string answer;
    bool ret = default_yes;

    const string CHOICES = default_yes ? " [Y/n] " : " [y/N] ";
    cout << fg::cyan << question << fg::reset << CHOICES;
    getline(cin, answer);
    // Normalize input
    ranges::transform(answer, answer.begin(), ::tolower);
    if (!answer.empty()) {
        ret = answer == "y" || answer == "yes" || answer == "1";
    }
    SPDLOG_DEBUG("CLI CONFIRM -> User input: {}; interpreted as: {}", answer, ret);
    return ret;
} // end of confirm function


void Cli::confirmExit() {
    SPDLOG_DEBUG("CLI CONFIRM EXIT -> Asking user to press Enter to exit the program");
    cout << "Press " << style::blink << fg::red << "Enter" << style::reset << " to exit the program." << '\n';
    string dummy;
    getline(cin, dummy);
    SPDLOG_DEBUG("CLI CONFIRM EXIT -> Confirmed");
} // end of confirm_exit function


string Cli::prompt(const string &question, const string &default_value) {
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


int Cli::menu(const string &title, const vector<string> &options, const bool END_LINE) {
    string user_choice{};
    int choice_int{};
    SPDLOG_DEBUG("CLI MENU -> {}; options: {}", title, options);
    // Display the menu options
    cout << style::underline << "--- " << title << " ---" << style::reset << '\n';
    for (size_t i = 0; i < options.size(); ++i) {
        cout << i + 1 << ". " << options.at(i) << '\n';
    }
    user_choice = prompt("Select a number corresponding to an option");
    try {
        choice_int = stoi(user_choice);
        if (choice_int < 1 || std::cmp_greater(choice_int, options.size())) {
            throw invalid_argument("Choice out of range");
        }
    } catch (const invalid_argument &) {
        SPDLOG_ERROR(MENU_INVALID_CHOICE, choice_int);
        err(std::format(MENU_INVALID_CHOICE, choice_int));
        std::cout << '\n';
        return -2;
    }
    SPDLOG_DEBUG("CLI MENU -> User selected: {}", user_choice);
    if (END_LINE) {
        std::cout << '\n';
    }
    return choice_int;
} // end of menu function


///
/// Draw a progress bar in CLI, redraws a previous line.
/// @param PERCENTAGE Current progress percentage (0-100)
/// @param BAR_WIDTH Width of the progress bar in characters
static void drawProgressBar(const int PERCENTAGE, const int BAR_WIDTH) {
    const int POS = PERCENTAGE * BAR_WIDTH / 100;
    std::cout << "\033[1A\r["
            << std::string(POS, '=')
            << (POS < BAR_WIDTH ? ">" : "")
            << std::string(std::max(0, BAR_WIDTH - POS - 1), ' ')
            << "] "
            << PERCENTAGE << "%"
            << std::flush
            << '\n';
}


ProgressBar::ProgressBar(const ProgressBarConfig CONFIG)
    : total_(CONFIG.total), change_trigger_(CONFIG.min_percent_change_to_redraw), bar_width_(CONFIG.bar_width) {
    assert(total_ > 0);
    assert(change_trigger_ > 0);
    std::cout << '\n';
    drawProgressBar(0, bar_width_);
}


void ProgressBar::update(const int PROGRESS) {
    const float RATIO = static_cast<float>(PROGRESS) / static_cast<float>(total_);
    const int PERCENTAGE = static_cast<int>(RATIO * 100.0F);
    if (PERCENTAGE != prev_bucket_ && PERCENTAGE / change_trigger_ != prev_bucket_) {
        drawProgressBar(PERCENTAGE, bar_width_);
    }
    prev_bucket_ = PERCENTAGE / change_trigger_;
}
