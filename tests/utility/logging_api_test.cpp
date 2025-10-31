#include <wujihandcpp/utility/logging.hpp>

#include <gtest/gtest.h>

namespace wujihandcpp::logging {

class LoggingApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable file logging for tests
        setenv("WUJI_LOG_TO_FILE", "0", 1);
        setenv("WUJI_LOG_TO_CONSOLE", "0", 1);
    }

    void TearDown() override {
        unsetenv("WUJI_LOG_TO_FILE");
        unsetenv("WUJI_LOG_TO_CONSOLE");
    }
};

TEST_F(LoggingApiTest, SetLogToConsoleDoesNotCrash) {
    EXPECT_NO_THROW(set_log_to_console(true));
    EXPECT_NO_THROW(set_log_to_console(false));
}

TEST_F(LoggingApiTest, SetLogToFileDoesNotCrash) {
    EXPECT_NO_THROW(set_log_to_file(true));
    EXPECT_NO_THROW(set_log_to_file(false));
}

TEST_F(LoggingApiTest, SetLogLevelAcceptsAllLevels) {
    EXPECT_NO_THROW(set_log_level(Level::TRACE));
    EXPECT_NO_THROW(set_log_level(Level::DEBUG));
    EXPECT_NO_THROW(set_log_level(Level::INFO));
    EXPECT_NO_THROW(set_log_level(Level::WARN));
    EXPECT_NO_THROW(set_log_level(Level::ERR));
    EXPECT_NO_THROW(set_log_level(Level::CRITICAL));
    EXPECT_NO_THROW(set_log_level(Level::OFF));
}

TEST_F(LoggingApiTest, SetLogPathThrowsOnNullptr) {
    EXPECT_THROW(set_log_path(nullptr), std::invalid_argument);
}

TEST_F(LoggingApiTest, SetLogPathAcceptsValidPath) {
    // This will throw if logger is already constructed, which is expected
    try {
        set_log_path("/tmp/test_log");
        SUCCEED();
    } catch (const std::logic_error& e) {
        // Expected if logger already exists
        EXPECT_STREQ("It is illegal to set log path after the Logger is constructed", e.what());
    }
}

TEST_F(LoggingApiTest, LevelEnumHasCorrectValues) {
    EXPECT_EQ(0, static_cast<int>(Level::TRACE));
    EXPECT_EQ(1, static_cast<int>(Level::DEBUG));
    EXPECT_EQ(2, static_cast<int>(Level::INFO));
    EXPECT_EQ(3, static_cast<int>(Level::WARN));
    EXPECT_EQ(4, static_cast<int>(Level::ERR));
    EXPECT_EQ(5, static_cast<int>(Level::CRITICAL));
    EXPECT_EQ(6, static_cast<int>(Level::OFF));
}

TEST_F(LoggingApiTest, ApiIsNoexceptWhereExpected) {
    // These should be noexcept
    EXPECT_TRUE(noexcept(set_log_to_console(true)));
    EXPECT_TRUE(noexcept(set_log_to_file(true)));
    EXPECT_TRUE(noexcept(set_log_level(Level::INFO)));

    // set_log_path is not noexcept
    EXPECT_FALSE(noexcept(set_log_path("/tmp")));
}

TEST_F(LoggingApiTest, MultipleCallsToSettersWork) {
    for (int i = 0; i < 10; ++i) {
        EXPECT_NO_THROW(set_log_to_console(i % 2 == 0));
        EXPECT_NO_THROW(set_log_to_file(i % 2 == 1));
        EXPECT_NO_THROW(set_log_level(static_cast<Level>(i % 7)));
    }
}

} // namespace wujihandcpp::logging