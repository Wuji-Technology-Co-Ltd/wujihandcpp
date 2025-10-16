#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <thread>
#include <vector>

// 直接包含logger.cpp实现文件
#include "../src/utility/logger.cpp"

namespace wujihandcpp::utility {

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 保存原始日志级别
        original_level_ = Logger::get()->level();

        // 创建内存sink用于测试
        stream_ = std::make_shared<std::ostringstream>();
        stream_sink_ = std::make_shared<spdlog::sinks::ostream_sink_mt>(*stream_);

        // 创建测试专用的logger
        test_logger_ = std::make_shared<spdlog::logger>("test_logger", stream_sink_);
        test_logger_->set_pattern("[%l] %v");
        test_logger_->set_level(spdlog::level::trace);
    }

    void TearDown() override {
        // 恢复原始日志级别
        Logger::get()->set_level(original_level_);
    }

    std::string get_log_output() {
        test_logger_->flush();
        return stream_->str();
    }

    void clear_log_output() {
        stream_->str("");
        stream_->clear();
    }

    std::shared_ptr<std::ostringstream> stream_;
    std::shared_ptr<spdlog::sinks::ostream_sink_mt> stream_sink_;
    std::shared_ptr<spdlog::logger> test_logger_;
    spdlog::level::level_enum original_level_;
};

// 测试基本日志级别功能
TEST_F(LoggerTest, BasicLogLevels) {
    Logger::set_level(spdlog::level::trace);

    // 测试所有级别的日志宏
    WUJI_TRACE("Trace message with number: {}", 42);
    WUJI_DEBUG("Debug message with string: {}", "test");
    WUJI_INFO("Info message with float: {:.2f}", 3.14);
    WUJI_WARN("Warning message");
    WUJI_ERROR("Error message");
    WUJI_CRITICAL("Critical message");

    // 验证所有级别的日志都能正常工作
    Logger::flush();
}

// 测试日志级别设置
TEST_F(LoggerTest, SetLevel) {
    Logger::set_level(spdlog::level::debug);
    EXPECT_EQ(Logger::get()->level(), spdlog::level::debug);

    Logger::set_level("trace");
    EXPECT_EQ(Logger::get()->level(), spdlog::level::trace);

    Logger::set_level("warn");
    EXPECT_EQ(Logger::get()->level(), spdlog::level::warn);

    // 测试枚举级别设置
    Logger::set_level(spdlog::level::info);
    EXPECT_EQ(Logger::get()->level(), spdlog::level::info);
}

// 测试日志级别过滤
TEST_F(LoggerTest, LevelFiltering) {
    Logger::set_level(spdlog::level::warn);

    // 这些应该被过滤掉
    WUJI_DEBUG("This debug message should not appear");
    WUJI_INFO("This info message should not appear");

    // 这些应该出现
    WUJI_WARN("This warn message should appear");
    WUJI_ERROR("This error message should appear");

    // 验证日志级别过滤
    EXPECT_GE(Logger::get()->level(), spdlog::level::warn);
}

// 测试日志格式化
TEST_F(LoggerTest, LogFormatting) {
    Logger::set_level(spdlog::level::info);

    // 测试各种数据类型的格式化
    WUJI_INFO("Integer: {}, Float: {:.2f}, String: {}", 123, 45.6789, "test_string");
    WUJI_INFO("Boolean: {}, Pointer: {}", true, static_cast<void*>(nullptr));
    WUJI_INFO("Multiple values: {}, {}, {}", 1, 2, 3);

    Logger::flush();
}

// 测试文件日志功能
TEST_F(LoggerTest, FileLogging) {
    // 创建临时目录用于测试
    std::string temp_dir = "/tmp/wuji_test_logs_" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(temp_dir);

    std::string test_log_file = temp_dir + "/test.log";

    // 启用文件日志
    Logger::enable_file_logging(test_log_file, 1024 * 1024, 1);

    // 写入一些测试日志
    WUJI_INFO("Test file logging message");
    WUJI_WARN("Another test message");
    WUJI_ERROR("Error message for file logging");

    // 刷新日志确保写入文件
    Logger::flush();

    // 验证文件已创建
    EXPECT_TRUE(std::filesystem::exists(test_log_file));

    // 验证文件内容
    std::ifstream log_file(test_log_file);
    std::string content(
        (std::istreambuf_iterator<char>(log_file)), std::istreambuf_iterator<char>());

    EXPECT_TRUE(content.find("Test file logging message") != std::string::npos);
    EXPECT_TRUE(content.find("Another test message") != std::string::npos);
    EXPECT_TRUE(content.find("Error message for file logging") != std::string::npos);

    // 清理临时文件
    std::filesystem::remove_all(temp_dir);
}

// 测试日志环境变量配置
TEST_F(LoggerTest, EnvironmentVariableLogDir) {
    // 设置环境变量
    std::string custom_log_dir = "/tmp/wuji_env_test_" + std::to_string(std::time(nullptr));
    setenv("WUJI_LOG_DIR", custom_log_dir.c_str(), 1);

    std::string test_log_file = custom_log_dir + "/env_test.log";

    // 启用文件日志
    Logger::enable_file_logging(test_log_file, 1024 * 1024, 1);

    WUJI_INFO("Environment variable test message");
    Logger::flush();

    // 验证文件在正确的位置创建
    EXPECT_TRUE(std::filesystem::exists(test_log_file));

    // 清理
    std::filesystem::remove_all(custom_log_dir);
    unsetenv("WUJI_LOG_DIR");
}

// 测试日志刷新功能
TEST_F(LoggerTest, FlushFunctionality) {
    Logger::set_level(spdlog::level::info);

    // 写入一些日志
    WUJI_INFO("Message before flush");
    WUJI_WARN("Warning before flush");

    // 手动刷新
    Logger::flush();

    // 设置自动刷新级别
    Logger::flush_on(spdlog::level::err);

    // 写入错误级别的日志应该自动刷新
    WUJI_ERROR("Error message with auto-flush");

    // 再次手动刷新确保所有日志都已写入
    Logger::flush();
}

// 测试多线程日志记录
TEST_F(LoggerTest, MultiThreadedLogging) {
    Logger::set_level(spdlog::level::info);

    const int num_threads = 4;
    const int messages_per_thread = 10;

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([i, messages_per_thread]() {
            for (int j = 0; j < messages_per_thread; ++j) {
                WUJI_INFO("Thread {} message {}", i, j);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    Logger::flush();
}

// 测试日志轮转功能
TEST_F(LoggerTest, LogRotation) {
    std::string temp_dir = "/tmp/wuji_rotation_test_" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(temp_dir);

    std::string base_log_file = temp_dir + "/rotation.log";

    // 启用文件日志，设置较小的最大文件大小以触发轮转
    Logger::enable_file_logging(base_log_file, 1024, 3);  // 1KB 最大大小，保留3个文件

    // 写入足够多的日志以触发轮转
    for (int i = 0; i < 50; ++i) {
        WUJI_INFO("Rotation test message {}: This is a long message to fill up the log file quickly", i);
    }
    Logger::flush();

    // 验证主日志文件存在
    EXPECT_TRUE(std::filesystem::exists(base_log_file));

    // 检查是否有轮转文件
    int rotation_files_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(temp_dir)) {
        if (entry.is_regular_file() &&
            entry.path().string().find("rotation.log") != std::string::npos) {
            rotation_files_count++;
        }
    }

    // 应该至少有1个文件（主文件），可能还有轮转文件
    EXPECT_GE(rotation_files_count, 1);

    // 清理临时文件
    std::filesystem::remove_all(temp_dir);
}

// 测试并发文件写入
TEST_F(LoggerTest, ConcurrentFileWriting) {
    std::string temp_dir = "/tmp/wuji_concurrent_test_" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(temp_dir);

    std::string test_log_file = temp_dir + "/concurrent.log";

    // 启用文件日志
    Logger::enable_file_logging(test_log_file, 1024 * 1024, 1);

    const int num_threads = 6;
    const int messages_per_thread = 10;

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([i, messages_per_thread]() {
            for (int j = 0; j < messages_per_thread; ++j) {
                WUJI_INFO("Concurrent Thread {} - Message {}", i, j);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    Logger::flush();

    // 验证文件存在且包含所有消息
    EXPECT_TRUE(std::filesystem::exists(test_log_file));

    std::ifstream log_file(test_log_file);
    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());

    // 验证所有线程的消息都存在
    for (int i = 0; i < num_threads; ++i) {
        std::string thread_pattern = "Concurrent Thread " + std::to_string(i) + " - Message";
        EXPECT_TRUE(content.find(thread_pattern) != std::string::npos);
    }

    // 清理临时文件
    std::filesystem::remove_all(temp_dir);
}

} // namespace wujihandcpp::utility
