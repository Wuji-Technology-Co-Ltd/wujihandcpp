#include <wujihandcpp/utility/logger.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include "cross_os.hpp"

namespace wujihandcpp {
namespace utility {

/**
 * @brief Access the global shared logger for the wujihandcpp utility.
 *
 * Provides singleton-like access to the library's logger instance; the logger
 * is initialized on first use with the default console configuration.
 *
 * @return std::shared_ptr<spdlog::logger>& Reference to the shared_ptr that manages the global spdlog logger instance.
 */
API std::shared_ptr<spdlog::logger>& Logger::get() {
    static auto logger = create_default_logger();
    return logger;
}

/**
 * @brief Set the global logger's log level.
 *
 * @param level Log level to apply to the global logger.
 */
API void Logger::set_level(spdlog::level::level_enum level) {
    get()->set_level(level);
}

/**
 * @brief Set the global logger's verbosity using a textual level name.
 *
 * Converts the provided case-insensitive level name to a spdlog level and applies it
 * to the shared logger used by the application.
 *
 * @param level Textual log level recognized by spdlog (e.g. "trace", "debug", "info", "warn", "error", "critical", "off").
 */
API void Logger::set_level(const std::string& level) {
    auto log_level = spdlog::level::from_str(level);
    get()->set_level(log_level);
}

/**
 * @brief Enables rotating file logging in addition to the existing console sink.
 *
 * Replaces the global logger with a new logger that writes to both the original console sink
 * and a rotating file sink, preserves the current log level, and logs an informational message
 * indicating the file path on success; on failure an error is logged.
 *
 * @param file_path Path to the log file to write and rotate.
 * @param max_size Maximum size in bytes for a single log file before rotation.
 * @param max_files Maximum number of rotated log files to keep.
 */
API void Logger::enable_file_logging(
    const std::string& file_path,
    std::size_t max_size,
    std::size_t max_files
) {
    try {
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            file_path, max_size, max_files);

        auto console_sink = get()->sinks()[0];
        auto new_logger = std::make_shared<spdlog::logger>(
            "wujihandcpp",
            spdlog::sinks_init_list{console_sink, file_sink}
        );

        new_logger->set_level(get()->level());
        get() = new_logger;

        get()->info("Start writing to: {}", file_path);
    } catch (const spdlog::spdlog_ex& ex) {
        get()->error("Fail: {}", ex.what());
    }
}

/**
 * @brief Flushes all pending log messages from the global logger.
 *
 * Forces any buffered log records to be written immediately to their configured sinks.
 */
API void Logger::flush() {
    get()->flush();
}

/**
 * @brief Configure the global logger to flush its sinks when a message at or above the specified level is logged.
 *
 * @param level Logging level that triggers an automatic flush.
 */
API void Logger::flush_on(spdlog::level::level_enum level) {
    get()->flush_on(level);
}

/**
 * @brief Creates the default project logger configured for colored console output.
 *
 * Produces a logger named "wujihandcpp" with a timestamped, level-colored console pattern,
 * default log level set to info, and configured to flush on warn.
 *
 * @return std::shared_ptr<spdlog::logger> Logger instance configured with a color console sink, info level, and flush-on-warn behavior.
 */
std::shared_ptr<spdlog::logger> Logger::create_default_logger() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");

    auto logger = std::make_shared<spdlog::logger>("wujihandcpp", console_sink);
    logger->set_level(spdlog::level::info);
    logger->flush_on(spdlog::level::warn);

    return logger;
}

} // namespace utility
} // namespace wujihandcpp