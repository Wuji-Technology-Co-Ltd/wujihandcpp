#include <wujihandcpp/utility/logger.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace wujihandcpp {
namespace utility {

std::shared_ptr<spdlog::logger>& Logger::get() {
    static auto logger = create_default_logger();
    return logger;
}

void Logger::set_level(spdlog::level::level_enum level) {
    get()->set_level(level);
}

void Logger::set_level(const std::string& level) {
    auto log_level = spdlog::level::from_str(level);
    get()->set_level(log_level);
}

void Logger::enable_file_logging(
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

void Logger::flush() {
    get()->flush();
}

void Logger::flush_on(spdlog::level::level_enum level) {
    get()->flush_on(level);
}

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