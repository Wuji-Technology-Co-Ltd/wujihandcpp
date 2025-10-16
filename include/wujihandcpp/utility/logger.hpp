#pragma once

#include <spdlog/spdlog.h>
#include <memory>
#include <string>

namespace wujihandcpp {
namespace utility {

/**
 * @brief 现代化的日志管理器，基于 spdlog 实现
 */
class Logger {
public:
    static std::shared_ptr<spdlog::logger>& get();
    static void set_level(spdlog::level::level_enum level);
    static void set_level(const std::string& level);
    static void enable_file_logging(
        const std::string& file_path,
        std::size_t max_size = 1048576 * 10,  // 10MB
        std::size_t max_files = 3
    );
    static void flush();
    static void flush_on(spdlog::level::level_enum level);

private:
    static std::shared_ptr<spdlog::logger> create_default_logger();
};

} // namespace utility
} // namespace wujihandcpp

// 简洁的宏定义接口
#define WUJI_TRACE(...)    ::wujihandcpp::utility::Logger::get()->trace(__VA_ARGS__)
#define WUJI_DEBUG(...)    ::wujihandcpp::utility::Logger::get()->debug(__VA_ARGS__)
#define WUJI_INFO(...)     ::wujihandcpp::utility::Logger::get()->info(__VA_ARGS__)
#define WUJI_WARN(...)     ::wujihandcpp::utility::Logger::get()->warn(__VA_ARGS__)
#define WUJI_ERROR(...)    ::wujihandcpp::utility::Logger::get()->error(__VA_ARGS__)
#define WUJI_CRITICAL(...) ::wujihandcpp::utility::Logger::get()->critical(__VA_ARGS__)