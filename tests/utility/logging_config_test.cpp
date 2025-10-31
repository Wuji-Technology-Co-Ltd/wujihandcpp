#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#define private public
#include "utility/logging.hpp"
#undef private

#include <gtest/gtest.h>

namespace wujihandcpp::logging {

class LoggingConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Save original environment variables
        saved_console_ = std::getenv("WUJI_LOG_TO_CONSOLE");
        saved_file_ = std::getenv("WUJI_LOG_TO_FILE");
        saved_level_ = std::getenv("WUJI_LOG_LEVEL");
        saved_path_ = std::getenv("WUJI_LOG_PATH");
    }

    void TearDown() override {
        // Restore environment variables
        restore_env("WUJI_LOG_TO_CONSOLE", saved_console_);
        restore_env("WUJI_LOG_TO_FILE", saved_file_);
        restore_env("WUJI_LOG_LEVEL", saved_level_);
        restore_env("WUJI_LOG_PATH", saved_path_);
    }

private:
    void restore_env(const char* name, const char* value) {
        if (value) {
            setenv(name, value, 1);
        } else {
            unsetenv(name);
        }
    }

    const char* saved_console_;
    const char* saved_file_;
    const char* saved_level_;
    const char* saved_path_;
};

TEST_F(LoggingConfigTest, ParseBoolAcceptsTrueValues) {
    std::atomic<bool> destination{false};

    EXPECT_TRUE(Config::parse_bool(destination, "1"));
    EXPECT_TRUE(destination.load());

    destination.store(false);
    EXPECT_TRUE(Config::parse_bool(destination, "true"));
    EXPECT_TRUE(destination.load());

    destination.store(false);
    EXPECT_TRUE(Config::parse_bool(destination, "TRUE"));
    EXPECT_TRUE(destination.load());

    destination.store(false);
    EXPECT_TRUE(Config::parse_bool(destination, "on"));
    EXPECT_TRUE(destination.load());

    destination.store(false);
    EXPECT_TRUE(Config::parse_bool(destination, "ON"));
    EXPECT_TRUE(destination.load());

    destination.store(false);
    EXPECT_TRUE(Config::parse_bool(destination, "yes"));
    EXPECT_TRUE(destination.load());

    destination.store(false);
    EXPECT_TRUE(Config::parse_bool(destination, "YES"));
    EXPECT_TRUE(destination.load());
}

TEST_F(LoggingConfigTest, ParseBoolAcceptsFalseValues) {
    std::atomic<bool> destination{true};

    EXPECT_TRUE(Config::parse_bool(destination, "0"));
    EXPECT_FALSE(destination.load());

    destination.store(true);
    EXPECT_TRUE(Config::parse_bool(destination, "false"));
    EXPECT_FALSE(destination.load());

    destination.store(true);
    EXPECT_TRUE(Config::parse_bool(destination, "FALSE"));
    EXPECT_FALSE(destination.load());

    destination.store(true);
    EXPECT_TRUE(Config::parse_bool(destination, "off"));
    EXPECT_FALSE(destination.load());

    destination.store(true);
    EXPECT_TRUE(Config::parse_bool(destination, "OFF"));
    EXPECT_FALSE(destination.load());

    destination.store(true);
    EXPECT_TRUE(Config::parse_bool(destination, "no"));
    EXPECT_FALSE(destination.load());

    destination.store(true);
    EXPECT_TRUE(Config::parse_bool(destination, "NO"));
    EXPECT_FALSE(destination.load());
}

TEST_F(LoggingConfigTest, ParseBoolRejectsInvalidValues) {
    std::atomic<bool> destination{false};

    EXPECT_FALSE(Config::parse_bool(destination, nullptr));
    EXPECT_FALSE(Config::parse_bool(destination, ""));
    EXPECT_FALSE(Config::parse_bool(destination, "invalid"));
    EXPECT_FALSE(Config::parse_bool(destination, "2"));
    EXPECT_FALSE(Config::parse_bool(destination, "maybe"));
    EXPECT_FALSE(Config::parse_bool(destination, "True "));
    EXPECT_FALSE(Config::parse_bool(destination, " false"));
}

TEST_F(LoggingConfigTest, ParseLevelAcceptsAllLevels) {
    std::atomic<Level> destination{Level::OFF};

    EXPECT_TRUE(Config::parse_level(destination, "trace"));
    EXPECT_EQ(Level::TRACE, destination.load());

    EXPECT_TRUE(Config::parse_level(destination, "TRACE"));
    EXPECT_EQ(Level::TRACE, destination.load());

    EXPECT_TRUE(Config::parse_level(destination, "debug"));
    EXPECT_EQ(Level::DEBUG, destination.load());

    EXPECT_TRUE(Config::parse_level(destination, "DEBUG"));
    EXPECT_EQ(Level::DEBUG, destination.load());

    EXPECT_TRUE(Config::parse_level(destination, "info"));
    EXPECT_EQ(Level::INFO, destination.load());

    EXPECT_TRUE(Config::parse_level(destination, "INFO"));
    EXPECT_EQ(Level::INFO, destination.load());

    EXPECT_TRUE(Config::parse_level(destination, "information"));
    EXPECT_EQ(Level::INFO, destination.load());

    EXPECT_TRUE(Config::parse_level(destination, "warn"));
    EXPECT_EQ(Level::WARN, destination.load());

    EXPECT_TRUE(Config::parse_level(destination, "WARN"));
    EXPECT_EQ(Level::WARN, destination.load());

    EXPECT_TRUE(Config::parse_level(destination, "warning"));
    EXPECT_EQ(Level::WARN, destination.load());

    EXPECT_TRUE(Config::parse_level(destination, "err"));
    EXPECT_EQ(Level::ERR, destination.load());

    EXPECT_TRUE(Config::parse_level(destination, "ERR"));
    EXPECT_EQ(Level::ERR, destination.load());

    EXPECT_TRUE(Config::parse_level(destination, "error"));
    EXPECT_EQ(Level::ERR, destination.load());

    EXPECT_TRUE(Config::parse_level(destination, "critical"));
    EXPECT_EQ(Level::CRITICAL, destination.load());

    EXPECT_TRUE(Config::parse_level(destination, "CRITICAL"));
    EXPECT_EQ(Level::CRITICAL, destination.load());

    EXPECT_TRUE(Config::parse_level(destination, "crit"));
    EXPECT_EQ(Level::CRITICAL, destination.load());

    EXPECT_TRUE(Config::parse_level(destination, "off"));
    EXPECT_EQ(Level::OFF, destination.load());

    EXPECT_TRUE(Config::parse_level(destination, "OFF"));
    EXPECT_EQ(Level::OFF, destination.load());
}

TEST_F(LoggingConfigTest, ParseLevelRejectsInvalidValues) {
    std::atomic<Level> destination{Level::INFO};

    EXPECT_FALSE(Config::parse_level(destination, nullptr));
    EXPECT_FALSE(Config::parse_level(destination, ""));
    EXPECT_FALSE(Config::parse_level(destination, "invalid"));
    EXPECT_FALSE(Config::parse_level(destination, "informational"));
    EXPECT_FALSE(Config::parse_level(destination, "trace "));
    EXPECT_FALSE(Config::parse_level(destination, " debug"));
}

TEST_F(LoggingConfigTest, ToLowerStringConvertsCorrectly) {
    EXPECT_EQ("hello", Config::to_lower_string("hello"));
    EXPECT_EQ("hello", Config::to_lower_string("HELLO"));
    EXPECT_EQ("hello", Config::to_lower_string("HeLLo"));
    EXPECT_EQ("123", Config::to_lower_string("123"));
    EXPECT_EQ("test_value", Config::to_lower_string("Test_Value"));
    EXPECT_EQ("", Config::to_lower_string(""));
}

TEST_F(LoggingConfigTest, GetInstanceReturnsSameInstance) {
    auto& config1 = Config::get_instance();
    auto& config2 = Config::get_instance();
    EXPECT_EQ(&config1, &config2);
}

TEST_F(LoggingConfigTest, DefaultValuesAreReasonable) {
    auto& config = Config::get_instance();
    // Default values should be set (actual values depend on env vars)
    // Just check that methods work without crashing
    volatile bool console = config.log_to_console();
    volatile bool file = config.log_to_file();
    volatile Level level = config.log_level();
    (void)console;
    (void)file;
    (void)level;
    SUCCEED();
}

TEST_F(LoggingConfigTest, SettersUpdateValues) {
    auto& config = Config::get_instance();

    config.set_log_to_console(false);
    EXPECT_FALSE(config.log_to_console());

    config.set_log_to_console(true);
    EXPECT_TRUE(config.log_to_console());

    config.set_log_to_file(false);
    EXPECT_FALSE(config.log_to_file());

    config.set_log_to_file(true);
    EXPECT_TRUE(config.log_to_file());
}

TEST_F(LoggingConfigTest, SetLogLevelUpdatesLevel) {
    auto& config = Config::get_instance();

    config.set_log_level(Level::TRACE);
    EXPECT_EQ(Level::TRACE, config.log_level());

    config.set_log_level(Level::DEBUG);
    EXPECT_EQ(Level::DEBUG, config.log_level());

    config.set_log_level(Level::INFO);
    EXPECT_EQ(Level::INFO, config.log_level());

    config.set_log_level(Level::WARN);
    EXPECT_EQ(Level::WARN, config.log_level());

    config.set_log_level(Level::ERR);
    EXPECT_EQ(Level::ERR, config.log_level());

    config.set_log_level(Level::CRITICAL);
    EXPECT_EQ(Level::CRITICAL, config.log_level());

    config.set_log_level(Level::OFF);
    EXPECT_EQ(Level::OFF, config.log_level());
}

TEST_F(LoggingConfigTest, LogPathCanBeRetrieved) {
    auto& config = Config::get_instance();
    auto path = config.log_path();
    // Path should be retrievable (may be empty if home dir not found)
    SUCCEED();
}

TEST_F(LoggingConfigTest, AtomicOperationsAreThreadSafe) {
    auto& config = Config::get_instance();
    std::atomic<bool> start{false};
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&, i]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int j = 0; j < 100; ++j) {
                config.set_log_level(static_cast<Level>(j % 7));
                volatile Level level = config.log_level();
                (void)level;
                config.set_log_to_console(j % 2 == 0);
                volatile bool console = config.log_to_console();
                (void)console;
            }
        });
    }

    start.store(true, std::memory_order_release);

    for (auto& thread : threads) {
        thread.join();
    }

    SUCCEED();
}

} // namespace wujihandcpp::logging