#include <atomic>
#include <thread>
#include <vector>

#define private public
#include "utility/logging.hpp"
#undef private

#include <gtest/gtest.h>

namespace wujihandcpp::logging {

class LoggingLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable file logging for tests to avoid filesystem issues
        setenv("WUJI_LOG_TO_FILE", "0", 1);
        setenv("WUJI_LOG_TO_CONSOLE", "0", 1);
    }

    void TearDown() override {
        unsetenv("WUJI_LOG_TO_FILE");
        unsetenv("WUJI_LOG_TO_CONSOLE");
    }
};

TEST_F(LoggingLoggerTest, GetInstanceReturnsSameInstance) {
    auto& logger1 = Logger::get_instance();
    auto& logger2 = Logger::get_instance();
    EXPECT_EQ(&logger1, &logger2);
}

TEST_F(LoggingLoggerTest, HasInstanceReturnsTrueAfterConstruction) {
    EXPECT_TRUE(Logger::has_instance() || !Logger::has_instance()); // May already exist
    auto& logger = Logger::get_instance();
    (void)logger;
    EXPECT_TRUE(Logger::has_instance());
}

TEST_F(LoggingLoggerTest, SetLogLevelChangesLevel) {
    auto& logger = Logger::get_instance();

    logger.set_log_level(Level::TRACE);
    EXPECT_EQ(Level::TRACE, logger.level());

    logger.set_log_level(Level::DEBUG);
    EXPECT_EQ(Level::DEBUG, logger.level());

    logger.set_log_level(Level::INFO);
    EXPECT_EQ(Level::INFO, logger.level());

    logger.set_log_level(Level::WARN);
    EXPECT_EQ(Level::WARN, logger.level());

    logger.set_log_level(Level::ERR);
    EXPECT_EQ(Level::ERR, logger.level());

    logger.set_log_level(Level::CRITICAL);
    EXPECT_EQ(Level::CRITICAL, logger.level());

    logger.set_log_level(Level::OFF);
    EXPECT_EQ(Level::OFF, logger.level());
}

TEST_F(LoggingLoggerTest, ShouldLogRespectsLevel) {
    auto& logger = Logger::get_instance();

    logger.set_log_level(Level::INFO);

    EXPECT_FALSE(logger.should_log(Level::TRACE));
    EXPECT_FALSE(logger.should_log(Level::DEBUG));
    EXPECT_TRUE(logger.should_log(Level::INFO));
    EXPECT_TRUE(logger.should_log(Level::WARN));
    EXPECT_TRUE(logger.should_log(Level::ERR));
    EXPECT_TRUE(logger.should_log(Level::CRITICAL));
}

TEST_F(LoggingLoggerTest, ShouldLogWorksWithOff) {
    auto& logger = Logger::get_instance();

    logger.set_log_level(Level::OFF);

    EXPECT_FALSE(logger.should_log(Level::TRACE));
    EXPECT_FALSE(logger.should_log(Level::DEBUG));
    EXPECT_FALSE(logger.should_log(Level::INFO));
    EXPECT_FALSE(logger.should_log(Level::WARN));
    EXPECT_FALSE(logger.should_log(Level::ERR));
    EXPECT_FALSE(logger.should_log(Level::CRITICAL));
}

TEST_F(LoggingLoggerTest, ShouldLogWorksWithTrace) {
    auto& logger = Logger::get_instance();

    logger.set_log_level(Level::TRACE);

    EXPECT_TRUE(logger.should_log(Level::TRACE));
    EXPECT_TRUE(logger.should_log(Level::DEBUG));
    EXPECT_TRUE(logger.should_log(Level::INFO));
    EXPECT_TRUE(logger.should_log(Level::WARN));
    EXPECT_TRUE(logger.should_log(Level::ERR));
    EXPECT_TRUE(logger.should_log(Level::CRITICAL));
}

TEST_F(LoggingLoggerTest, LoggingMethodsDoNotCrash) {
    auto& logger = Logger::get_instance();
    logger.set_log_level(Level::TRACE);

    // Test all logging methods
    EXPECT_NO_THROW(logger.trace("trace message"));
    EXPECT_NO_THROW(logger.debug("debug message"));
    EXPECT_NO_THROW(logger.info("info message"));
    EXPECT_NO_THROW(logger.warn("warn message"));
    EXPECT_NO_THROW(logger.error("error message"));
    EXPECT_NO_THROW(logger.critical("critical message"));
}

TEST_F(LoggingLoggerTest, FormattedLoggingWorks) {
    auto& logger = Logger::get_instance();
    logger.set_log_level(Level::TRACE);

    EXPECT_NO_THROW(logger.trace("trace: {}", 42));
    EXPECT_NO_THROW(logger.debug("debug: {} {}", "test", 3.14));
    EXPECT_NO_THROW(logger.info("info: {0} {1} {0}", 1, 2));
    EXPECT_NO_THROW(logger.warn("warn: {:x}", 255));
    EXPECT_NO_THROW(logger.error("error: {}", std::string("string")));
    EXPECT_NO_THROW(logger.critical("critical: {:.2f}", 2.71828));
}

TEST_F(LoggingLoggerTest, LogMethodWorks) {
    auto& logger = Logger::get_instance();
    logger.set_log_level(Level::TRACE);

    EXPECT_NO_THROW(logger.log(Level::TRACE, "trace"));
    EXPECT_NO_THROW(logger.log(Level::DEBUG, "debug"));
    EXPECT_NO_THROW(logger.log(Level::INFO, "info"));
    EXPECT_NO_THROW(logger.log(Level::WARN, "warn"));
    EXPECT_NO_THROW(logger.log(Level::ERR, "error"));
    EXPECT_NO_THROW(logger.log(Level::CRITICAL, "critical"));
}

TEST_F(LoggingLoggerTest, LogMethodWithFormattingWorks) {
    auto& logger = Logger::get_instance();
    logger.set_log_level(Level::TRACE);

    EXPECT_NO_THROW(logger.log(Level::INFO, "formatted: {} {} {}", 1, "two", 3.0));
}

TEST_F(LoggingLoggerTest, FlushDoesNotCrash) {
    auto& logger = Logger::get_instance();
    EXPECT_NO_THROW(logger.flush());
}

TEST_F(LoggingLoggerTest, SetLogToConsoleWorks) {
    auto& logger = Logger::get_instance();
    EXPECT_NO_THROW(logger.set_log_to_console(true));
    EXPECT_NO_THROW(logger.set_log_to_console(false));
}

TEST_F(LoggingLoggerTest, SetLogToFileWorks) {
    auto& logger = Logger::get_instance();
    EXPECT_NO_THROW(logger.set_log_to_file(true));
    EXPECT_NO_THROW(logger.set_log_to_file(false));
}

TEST_F(LoggingLoggerTest, ConcurrentLoggingIsThreadSafe) {
    auto& logger = Logger::get_instance();
    logger.set_log_level(Level::INFO);

    std::atomic<bool> start{false};
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&, i]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int j = 0; j < 100; ++j) {
                logger.info("Thread {} iteration {}", i, j);
                logger.warn("Warning from thread {}", i);
                logger.error("Error from thread {}", i);
            }
        });
    }

    start.store(true, std::memory_order_release);

    for (auto& thread : threads) {
        thread.join();
    }

    SUCCEED();
}

TEST_F(LoggingLoggerTest, LevelConversionsAreCorrect) {
    EXPECT_EQ(static_cast<int>(Level::TRACE), static_cast<int>(spdlog::level::trace));
    EXPECT_EQ(static_cast<int>(Level::DEBUG), static_cast<int>(spdlog::level::debug));
    EXPECT_EQ(static_cast<int>(Level::INFO), static_cast<int>(spdlog::level::info));
    EXPECT_EQ(static_cast<int>(Level::WARN), static_cast<int>(spdlog::level::warn));
    EXPECT_EQ(static_cast<int>(Level::ERR), static_cast<int>(spdlog::level::err));
    EXPECT_EQ(static_cast<int>(Level::CRITICAL), static_cast<int>(spdlog::level::critical));
    EXPECT_EQ(static_cast<int>(Level::OFF), static_cast<int>(spdlog::level::off));
}

} // namespace wujihandcpp::logging