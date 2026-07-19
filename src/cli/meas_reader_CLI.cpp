#include "NPET_comm.h"
#include "meas_reader_CLI.h"

#include <cmath>

#include "cli.h"

constexpr std::string_view MEAS_START = "Starting measurement sequence...";
constexpr std::string_view MEAS_END = "Measurement sequence ended";
constexpr std::string_view MISSING_CONST = "Time correction in NPET is missing or invalid";
constexpr std::string_view CORRUPTED_MEAS_NUM = "Number of corrupted measurements: {}";
constexpr std::string_view SYNC_MONITOR = "Synchronization monitoring";
constexpr std::string_view ADVANCED_MONITOR = "Advanced monitoring";
constexpr std::string_view BASIC_MONITOR = "Basic monitoring";
constexpr std::string_view ALL_SAVED = "All measurements saved";
constexpr std::string_view TOO_MANY_LEFT_IN_BUFFER =
    "Measurement has already ended, data left in monitoring buffer have been dropped.";

///
/// Print an introduction message to the console at the start of the measurement sequence
/// @param meas_set Reference to the measurement context
/// @param time_const Reference to the time correction constant imported from the NPET device
void print_intro(const meas_context& meas_set, const measurement& time_const)
{
    SPDLOG_DEBUG(MEAS_START);
    cli::echo(MEAS_START.data(), fg::green);
    if (time_const.is_empty() || !time_const.is_valid())
    {
        SPDLOG_ERROR(MISSING_CONST);
        cli::err(MISSING_CONST.data());
    }
    if (meas_set.num_of_meas == INFINITE_OP) cli::echo("Reading infinite measurements...");
    else cli::show_int("Reading measurement(s)", meas_set.num_of_meas);
    cli::show_int("Using channel", meas_set.channel);
    cli::echo("Press `Esc` to safely cancel the measurement at any time", fg::gray, style::bold);
} // end of print_intro function


///
/// Print measurement end message, including the number of corrupt measurements.
/// @param reader Reference to the measurement_reader object that is reading measurements from the NPET device
/// @param meas_set Reference to the measurement context
void print_outro(meas_reader& reader, const meas_context& meas_set)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (const size_t saver_initial = reader.saver_q_size(); meas_set.save && saver_initial > 0)
    {
        SPDLOG_DEBUG("There are unsaved measurements left, saving them now");
        cli::echo("Saving data to file, do NOT close the application ...");
        SPDLOG_INFO("Number of measurements left to save: {}", saver_initial);
        size_t saver_remaining = saver_initial;
        auto bar = ProgressBar(static_cast<int>(saver_initial));
        while (saver_remaining > 0)
        {
            saver_remaining = reader.saver_q_size();
            bar.update(static_cast<int>(saver_initial - saver_remaining));
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
        } // end of while loop to wait for saver thread
        bar.update(static_cast<int>(saver_initial));
        SPDLOG_INFO(ALL_SAVED);
        cli::echo(ALL_SAVED.data(), fg::green);
    }
    SPDLOG_DEBUG(MEAS_END);
    cli::echo(MEAS_END.data(), fg::green);
    if (const int corrupted = reader.corrupted.load(std::memory_order_relaxed); corrupted > 0)
    {
        SPDLOG_ERROR(CORRUPTED_MEAS_NUM, corrupted);
        cli::err(std::format(CORRUPTED_MEAS_NUM, corrupted));
    }
} // end of print_outro


///
/// Convert a total number of seconds into hours, minutes, and seconds.
/// @param total_seconds Measured number of seconds to convert into hours, minutes, and seconds
/// @return Constexpr tuple of hours, minutes, and seconds corresponding to the total number of seconds
constexpr inline std::tuple<int, int, int>
to_hms(int total_seconds) noexcept
{
    const int hours = total_seconds / 3600;
    total_seconds %= 3600;
    const int minutes = total_seconds / 60;
    const int seconds = total_seconds % 60;
    return {hours, minutes, seconds};
} // end of to_hms function


///
/// @param hours Measurement hours to display
/// @param minutes Measurement minutes to display
/// @param seconds Measurement seconds to display
/// @param fractional_part Measurement fractional part of a second to display
/// @return Formatted string of the measurement in the format "hh:mm:ss fractional_part"
std::string format_measurement(
    const int hours,
    const int minutes,
    const int seconds,
    const std::string& fractional_part
)
{
    return std::format("{:02}:{:02}:{:02} {}", hours, minutes, seconds, fractional_part);
}


///
/// Display the measurement read from NPET only as hh:mm:ss with the fractional part rounded.
/// @param reader Reference to the measurement_reader object that is reading measurements from the NPET device
/// @param meas_set Reference to the measurement context
/// @param time_const Reference to the time correction constant imported from the NPET device
void reader_cli_sync(meas_reader& reader, const meas_context& meas_set, const measurement& time_const)
{
    print_intro(meas_set, time_const);
    SPDLOG_DEBUG(SYNC_MONITOR);
    cli::echo(SYNC_MONITOR.data());
    while (true)
    {
        const std::optional<measurement> meas = reader.grab_meas_from_processor(reader.for_monitor_q);
        if (!meas) break;
        // Convert the rounded measurement into hours, minutes, and seconds
        auto [hours, minutes, seconds] = to_hms(meas->round());
        cli::echo(format_measurement(hours, minutes, seconds, ""));
    } // end of while loop
    print_outro(reader, meas_set);
} // end of reader_cli_cli function


void reader_cli_advanced(meas_reader& reader, const meas_context& meas_set, const measurement& time_const)
{
    // Initialized progress string
    std::string progress;
    // Number of processed measurements
    int meas_count = 0;
    // Buffer to store the last 5 measurements for display
    std::array<meas_extended, 5> buffer5{};
    // Index to keep track of the current position in the buffer
    int index = 0;
    // Number of lines printed in the last iteration, the cursor is moved up by this number
    int total_lines = -1;

    print_intro(meas_set, time_const);
    SPDLOG_DEBUG(ADVANCED_MONITOR);
    cli::echo(ADVANCED_MONITOR.data());
    while (true)
    {
        const std::optional<measurement> meas = reader.grab_meas_from_processor(reader.for_monitor_q);
        if (!meas) break;
        // Increment the measurement count
        meas_count++;
        // Format the measurement display string
        meas_extended meas_ext(meas.value());
        auto [hours, minutes, seconds] = to_hms(meas->intp);
        // Format the progress information
        if (meas_set.num_of_meas == INFINITE_OP) progress = std::to_string(meas_count);
        else progress = std::to_string(meas_count * 100 / meas_set.num_of_meas) + "%";
        progress.insert(0, " [");
        progress += ']';
        meas_ext.processed_str = format_measurement(hours, minutes, seconds, float128_to_string(meas->fracp)) +
            progress;
        // Store values in a scrolling buffer
        buffer5[index] = meas_ext;
        index = (index + 1) % 5; // wraps the index around at 5 to create a circular buffer
        // Display the measurement information in CLI
        // Move cursor up by total_lines at the start of the iteration
        std::cout << "\033[" << total_lines << "A"; // move up N lines
        std::cout << "Measurement status: " << (
            reader.stop_sign.load(std::memory_order_relaxed) ? "Stopped" : "Running");
        std::cout << std::endl;
        // Calculate measurement frequency
        meas_extended& oldest = buffer5[index % 5];
        meas_extended& newest = buffer5[(index + 4) % 5];
        const int freq = std::lround(4 / (newest.approx_value() - oldest.approx_value()));
        std::cout << "Measurement frequency: " << freq << "  ";
        std::cout << std::endl;
        std::cout << "Corrupted measurements: " << reader.corrupted.load(std::memory_order_relaxed);
        std::cout << std::endl;
        std::cout << "Buffers ... Processor: " << reader.receiver_q_size();
        std::cout << " | Monitor: " << reader.monitor_q_size();
        std::cout << " | Saver: " << reader.saver_q_size();
        std::cout << "            " << std::endl; // Clear line from the previous iter
        total_lines = 4; // the status + frequency + corrupted + buffers
        // Read all values in order (oldest to newest)
        for (int i = 0; i < 5; i++)
        {
            const meas_extended& m = buffer5[(index + i) % 5];
            if (!m.is_processed() || m.is_empty()) continue; // Skip empty buffer slots
            std::cout << m.processed_str << std::endl;
            total_lines++;
        } // end of for loop to display buffer values
        std::cout.flush(); // important if no newline
        // The measurement has already ended and there are too many measurements to print still, the buffer is dropped
        if (reader.stop_sign.load(std::memory_order_relaxed) && static_cast<double>(reader.monitor_q_size()) > 0.1 *
            meas_set.num_of_meas && meas_set.save)
        {
            cli::echo(TOO_MANY_LEFT_IN_BUFFER.data(), fg::yellow);
            SPDLOG_DEBUG(TOO_MANY_LEFT_IN_BUFFER);
            break;
        }
    } // end of while loop
    print_outro(reader, meas_set);
} // end of reader_cli_advanced function

///
/// Display only a progress bar of the measurement process.
/// @param reader Reference to the measurement_reader object that is reading measurements from the NPET device
/// @param meas_set Reference to the measurement context
/// @param time_const Reference to the time correction constant imported from the NPET device
void reader_cli_basic(meas_reader& reader, const meas_context& meas_set, const measurement& time_const)
{
    print_intro(meas_set, time_const);
    SPDLOG_DEBUG(BASIC_MONITOR);
    cli::echo(BASIC_MONITOR.data());
    int measurement_num{};
    auto bar = ProgressBar(meas_set.num_of_meas);
    while (true)
    {
        // If the measurement has already ended and there are too many measurements to print still, drop them
        if (reader.stop_sign.load(std::memory_order_relaxed) && reader.monitor_q_size() > 200) break;
        if (const std::optional<measurement> meas = reader.grab_meas_from_processor(reader.for_monitor_q); !meas) break;
        measurement_num++;
        bar.update(measurement_num);
    } // end of while loop
    print_outro(reader, meas_set);
} // end of reader_cli_basic function
