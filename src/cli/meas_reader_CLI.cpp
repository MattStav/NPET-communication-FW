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
constexpr std::string_view DUAL_MEAS_START = "Starting dual measurement sequence...";
constexpr std::string_view DUAL_MEAS_END = "Dual measurement sequence ended";
constexpr std::string_view DUAL_BASIC_MONITOR = "Basic dual monitoring";

///
/// Print an introduction message to the console at the start of the measurement sequence
/// @param meas_set Reference to the measurement context
/// @param time_const Reference to the time correction constant imported from the NPET device
static void printIntro(const MeasContext &meas_set, const Measurement &time_const) {
    SPDLOG_DEBUG(MEAS_START);
    Cli::echo(std::string(MEAS_START), fg::green);
    if (time_const.isEmpty() || !time_const.isValid()) {
        SPDLOG_ERROR(MISSING_CONST);
        Cli::err(std::string(MISSING_CONST));
    }
    if (meas_set.num_of_meas == INFINITE_OPERATION) {
        Cli::echo("Reading infinite measurements...");
    } else {
        Cli::showInt("Reading measurement(s)", meas_set.num_of_meas);
    }
    Cli::showInt("Using channel", static_cast<int>(meas_set.channel));
    Cli::echo("Press `Esc` to safely cancel the measurement at any time", fg::gray, style::bold);
} // end of print_intro function


///
/// Print measurement end message, including the number of corrupt measurements.
/// @param reader Reference to the measurement_reader object that is reading measurements from the NPET device
/// @param meas_set Reference to the measurement context
static void printOutro(MeasReader &reader, const MeasContext &meas_set) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (const size_t SAVER_INITIAL = reader.saverQSize(); meas_set.save_path && SAVER_INITIAL > 0) {
        SPDLOG_DEBUG("There are unsaved measurements left, saving them now");
        Cli::echo("Saving data to file, do NOT close the application ...");
        SPDLOG_INFO("Number of measurements left to save: {}", SAVER_INITIAL);
        size_t saver_remaining = SAVER_INITIAL;
        auto bar = ProgressBar({.total = static_cast<int>(SAVER_INITIAL)});
        while (saver_remaining > 0) {
            saver_remaining = reader.saverQSize();
            bar.update(static_cast<int>(SAVER_INITIAL - saver_remaining));
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
        } // end of while loop to wait for saver thread
        bar.update(static_cast<int>(SAVER_INITIAL));
        SPDLOG_INFO(ALL_SAVED);
        Cli::echo(std::string(ALL_SAVED), fg::green);
    }
    SPDLOG_DEBUG(MEAS_END);
    Cli::echo(std::string(MEAS_END), fg::green);
    if (const int CORRUPTED = reader.corrupted.load(std::memory_order_relaxed); CORRUPTED > 0) {
        SPDLOG_ERROR(CORRUPTED_MEAS_NUM, CORRUPTED);
        Cli::err(std::format(CORRUPTED_MEAS_NUM, CORRUPTED));
    }
} // end of print_outro


///
/// Convert a total number of seconds into hours, minutes, and seconds.
/// @param total_seconds Measured number of seconds to convert into hours, minutes, and seconds
/// @return Constexpr tuple of hours, minutes, and seconds corresponding to the total number of seconds
static constexpr std::tuple<int, int, int>
toHms(int total_seconds) noexcept {
    const int HOURS = total_seconds / 3600;
    total_seconds %= 3600;
    const int MINUTES = total_seconds / 60;
    const int SECONDS = total_seconds % 60;
    return {HOURS, MINUTES, SECONDS};
} // end of to_hms function


///
/// @param HOURS Measurement hours to display
/// @param MINUTES Measurement minutes to display
/// @param SECONDS Measurement seconds to display
/// @param fractional_part Measurement fractional part of a second to display
/// @return Formatted string of the measurement in the format "hh:mm:ss fractional_part"
static std::string formatMeasurement(
    const int HOURS,
    const int MINUTES,
    const int SECONDS,
    const std::string &fractional_part
) {
    return std::format("{:02}:{:02}:{:02} {}", HOURS, MINUTES, SECONDS, fractional_part);
}


void readerCliSync(MeasReader &reader, const MeasContext &meas_set, const Measurement &time_const) {
    printIntro(meas_set, time_const);
    SPDLOG_DEBUG(SYNC_MONITOR);
    Cli::echo(std::string(SYNC_MONITOR));
    while (!reader.aborted.load(std::memory_order_relaxed)) {
        const std::optional<Measurement> MEAS = reader.grabMeasFromProcessor(reader.for_monitor_q);
        if (!MEAS) {
            break;
        }
        // Convert the rounded measurement into hours, minutes, and seconds
        auto [hours, minutes, seconds] = toHms(MEAS->round());
        Cli::echo(formatMeasurement(hours, minutes, seconds, ""));
    } // end of while loop
    printOutro(reader, meas_set);
} // end of readerCliSync function


void readerCliAdvanced(MeasReader &reader, const MeasContext &meas_set, const Measurement &time_const) {
    // Initialized progress string
    std::string progress;
    // Number of processed measurements
    int meas_count = 0;
    // Buffer to store the last 5 measurements for display
    std::array<MeasExtended, 5> buffer5{};
    // Index to keep track of the current position in the buffer
    int index = 0;
    // Number of lines printed in the last iteration, the cursor is moved up by this number
    int total_lines = -1;

    printIntro(meas_set, time_const);
    SPDLOG_DEBUG(ADVANCED_MONITOR);
    Cli::echo(std::string(ADVANCED_MONITOR));
    while (!reader.aborted.load(std::memory_order_relaxed)) {
        const std::optional<Measurement> MEAS = reader.grabMeasFromProcessor(reader.for_monitor_q);
        if (!MEAS) {
            break;
        }
        // Increment the measurement count
        meas_count++;
        // Format the measurement display string
        MeasExtended meas_ext(MEAS.value());
        auto [hours, minutes, seconds] = toHms(MEAS->intp);
        // Format the progress information
        if (meas_set.num_of_meas == INFINITE_OPERATION) {
            progress = std::to_string(meas_count);
        } else {
            progress = std::to_string(meas_count * 100 / meas_set.num_of_meas) + "%";
        }
        progress.insert(0, " [");
        progress += ']';
        meas_ext.processed_str = formatMeasurement(hours, minutes, seconds, float128ToString(MEAS->fracp)) +
                                 progress;
        // Store values in a scrolling buffer
        buffer5.at(index) = meas_ext;
        index = (index + 1) % 5; // wraps the index around at 5 to create a circular buffer
        // Display the measurement information in CLI
        // Move cursor up by total_lines at the start of the iteration
        std::cout << "\033[" << total_lines << "A"; // move up N lines
        std::cout << "Measurement status: " << (
            reader.stop_sign.load(std::memory_order_relaxed) ? "Stopped" : "Running");
        std::cout << '\n';
        // Calculate measurement frequency
        MeasExtended const &oldest = buffer5.at(index % 5);
        MeasExtended const &newest = buffer5.at((index + 4) % 5);
        const int FREQ = std::lround(4 / (newest.approxValue() - oldest.approxValue()));
        std::cout << "Measurement frequency: " << FREQ << "  ";
        std::cout << '\n';
        std::cout << "Corrupted measurements: " << reader.corrupted.load(std::memory_order_relaxed);
        std::cout << '\n';
        std::cout << "Buffers ... Processor: " << reader.receiverQSize();
        std::cout << " | Monitor: " << reader.monitorQSize();
        std::cout << " | Saver: " << reader.saverQSize();
        std::cout << "            " << '\n'; // Clear line from the previous iter
        total_lines = 4; // the status + frequency + corrupted + buffers
        // Read all values in order (oldest to newest)
        for (int i = 0; i < 5; i++) {
            const MeasExtended &m = buffer5.at((index + i) % 5);
            if (!m.isProcessed() || m.isEmpty()) {
                continue; // Skip empty buffer slots
            }
            std::cout << m.processed_str << '\n';
            total_lines++;
        } // end of for loop to display buffer values
        std::cout.flush(); // important if no newline.
    } // end of while loop
    printOutro(reader, meas_set);
} // end of reader_cli_advanced function


///
/// Basic CLI measurement monitor for a finite number of measurements
/// @param reader Reference to the measurement_reader object that is reading measurements from the NPET device
/// @param NUM_OF_MEAS Number of measurements to read from the NPET device
static void readerCliBasicNonInfMeas(MeasReader &reader, const int NUM_OF_MEAS) {
    int measurement_num{};
    auto bar = ProgressBar({.total = NUM_OF_MEAS});
    while (true) {
        // If the measurement was aborted, stop printing immediately
        if (reader.aborted.load(std::memory_order_relaxed)) {
            break;
        }
        if (const std::optional<Measurement> MEAS = reader.grabMeasFromProcessor(reader.for_monitor_q); !MEAS) {
            break;
        }
        measurement_num++;
        bar.update(measurement_num);
    } // end of while loop
} // end of reader_cli_basic_non_inf_meas function


///
/// Basic CLI measurement monitor for an infinite number of measurements
/// @param reader Reference to the measurement_reader object that is reading measurements from the NPET device
static void readerCliBasicInfMeas(const MeasReader &reader) {
    int i = 0;
    while (!reader.aborted.load(std::memory_order_relaxed)) {
        constexpr std::array FRAMES = {'|', '/', '-', '\\'};
        std::cout << "\rMeasurement running ... " << FRAMES.at(i++ % 4) << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    } // end of while loop
    std::cout << '\n'; // Newline required
} // end of reader_cli_basic_inf_meas function


void readerCliBasic(MeasReader &reader, const MeasContext &meas_set, const Measurement &time_const) {
    printIntro(meas_set, time_const);
    SPDLOG_DEBUG(BASIC_MONITOR);
    Cli::echo(std::string(BASIC_MONITOR));
    if (meas_set.num_of_meas == INFINITE_OPERATION) {
        readerCliBasicInfMeas(reader);
    } else {
        readerCliBasicNonInfMeas(reader, meas_set.num_of_meas);
    }
    printOutro(reader, meas_set);
} // end of reader_cli_basic function


///
/// Print an introduction message to the console at the start of a dual measurement sequence
/// @param meas_set Reference to the dual measurement context
/// @param start_time_const Reference to the START leg's time correction constant
/// @param stop_time_const Reference to the STOP leg's time correction constant
static void printDualIntro(const DualMeasContext &meas_set, const Measurement &start_time_const,
                           const Measurement &stop_time_const) {
    SPDLOG_DEBUG(DUAL_MEAS_START);
    Cli::echo(std::string(DUAL_MEAS_START), fg::green);
    if (start_time_const.isEmpty() || !start_time_const.isValid() ||
        stop_time_const.isEmpty() || !stop_time_const.isValid()) {
        // TODO: Specify which const is missing
        SPDLOG_ERROR(MISSING_CONST);
        Cli::err(std::string(MISSING_CONST));
    }
    if (meas_set.num_of_meas == INFINITE_OPERATION) {
        Cli::echo("Reading infinite measurements...");
    } else {
        Cli::showInt("Reading measurement(s)", meas_set.num_of_meas);
    }
    Cli::showInt("Using START channel", static_cast<int>(meas_set.start_channel));
    Cli::showInt("Using STOP channel", static_cast<int>(meas_set.stop_channel));
    Cli::echo("Press `Esc` to safely cancel the measurement at any time", fg::gray, style::bold);
} // end of print_dual_intro function


///
/// Print dual measurement end message.
static void printDualOutro() {
    SPDLOG_DEBUG(DUAL_MEAS_END);
    Cli::echo(std::string(DUAL_MEAS_END), fg::green);
    // TODO: Add waiting for saver thread
    // TODO: Add information about corrupted measurements
    // TODO: Add information about unmatched measurements
} // end of print_dual_outro function


///
/// Basic CLI dual measurement monitor for a finite number of measurements
/// @param dual_reader Reference to the dual measurement reader combining both legs
/// @param NUM_OF_MEAS Number of measurements to read on each leg
static void dualReaderCliBasicNonInfMeas(DualMeasReader &dual_reader, const int NUM_OF_MEAS) {
    int measurement_num{};
    auto bar = ProgressBar({.total = NUM_OF_MEAS});
    while (dual_reader.grabMeasurement()) {
        measurement_num++;
        bar.update(measurement_num);
    } // end of while loop
} // end of dual_reader_cli_basic_non_inf_meas function


///
/// Basic CLI dual measurement monitor for an infinite number of measurements
/// @param dual_reader Reference to the dual measurement reader combining both legs
static void dualReaderCliBasicInfMeas(DualMeasReader &dual_reader) {
    int i = 0;
    while (dual_reader.grabMeasurement()) {
        constexpr std::array FRAMES = {'|', '/', '-', '\\'};
        std::cout << "\rMeasurement running ... " << FRAMES.at(i++ % 4) << std::flush;
    } // end of while loop
    std::cout << '\n'; // Newline required
} // end of dual_reader_cli_basic_inf_meas function


void dualReaderCliBasic(DualMeasReader &dual_reader, const DualMeasContext &meas_set,
                        const Measurement &start_time_const, const Measurement &stop_time_const) {
    printDualIntro(meas_set, start_time_const, stop_time_const);
    SPDLOG_DEBUG(DUAL_BASIC_MONITOR);
    Cli::echo(std::string(DUAL_BASIC_MONITOR));
    if (meas_set.num_of_meas == INFINITE_OPERATION) {
        dualReaderCliBasicInfMeas(dual_reader);
    } else {
        dualReaderCliBasicNonInfMeas(dual_reader, meas_set.num_of_meas);
    }
    printDualOutro();
} // end of dual_reader_cli_basic function
