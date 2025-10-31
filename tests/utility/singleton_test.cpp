#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#define private public
#include "utility/singleton.hpp"
#undef private

#include <gtest/gtest.h>

using namespace std::chrono_literals;

// Test class for singleton
class TestSingleton {
public:
    TestSingleton() : constructed_count_(++global_construct_count_) {}

    int get_constructed_count() const { return constructed_count_; }

    static int get_global_construct_count() { return global_construct_count_.load(); }

    static void reset_global_construct_count() { global_construct_count_.store(0); }

private:
    int constructed_count_;
    static inline std::atomic<int> global_construct_count_{0};
};

class SingletonTest : public ::testing::Test {
protected:
    void SetUp() override { TestSingleton::reset_global_construct_count(); }
};

TEST_F(SingletonTest, GetInstanceReturnsValidReference) {
    auto& instance = Singleton<TestSingleton>::get_instance();
    EXPECT_EQ(1, instance.get_constructed_count());
    EXPECT_EQ(1, TestSingleton::get_global_construct_count());
}

TEST_F(SingletonTest, MultipleCallsReturnSameInstance) {
    auto& instance1 = Singleton<TestSingleton>::get_instance();
    auto& instance2 = Singleton<TestSingleton>::get_instance();
    auto& instance3 = Singleton<TestSingleton>::get_instance();

    EXPECT_EQ(&instance1, &instance2);
    EXPECT_EQ(&instance2, &instance3);
    EXPECT_EQ(1, TestSingleton::get_global_construct_count());
}

TEST_F(SingletonTest, HasInstanceReturnsFalseInitially) {
    EXPECT_FALSE(Singleton<TestSingleton>::has_instance());
}

TEST_F(SingletonTest, HasInstanceReturnsTrueAfterConstruction) {
    EXPECT_FALSE(Singleton<TestSingleton>::has_instance());
    auto& instance = Singleton<TestSingleton>::get_instance();
    (void)instance;
    EXPECT_TRUE(Singleton<TestSingleton>::has_instance());
}

TEST_F(SingletonTest, AcquireInstanceMutexCanBeLocked) {
    auto lock = Singleton<TestSingleton>::acquire_instance_mutex();
    EXPECT_TRUE(lock.owns_lock());
}

TEST_F(SingletonTest, AcquireInstanceMutexPreventsRaceConditions) {
    constexpr int kThreadCount = 10;
    std::atomic<int> ready_count{0};
    std::atomic<bool> start{false};
    std::vector<std::thread> threads;
    std::vector<int> construct_counts(kThreadCount);

    threads.reserve(kThreadCount);

    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&, i]() {
            ready_count.fetch_add(1, std::memory_order_relaxed);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            auto& instance = Singleton<TestSingleton>::get_instance();
            construct_counts[i] = instance.get_constructed_count();
        });
    }

    // Wait for all threads to be ready
    while (ready_count.load(std::memory_order_relaxed) < kThreadCount) {
        std::this_thread::yield();
    }

    start.store(true, std::memory_order_release);

    for (auto& thread : threads) {
        thread.join();
    }

    // All threads should see the same instance
    for (int count : construct_counts) {
        EXPECT_EQ(1, count);
    }
    EXPECT_EQ(1, TestSingleton::get_global_construct_count());
}

TEST_F(SingletonTest, MutexCanBeAcquiredMultipleTimes) {
    {
        auto lock1 = Singleton<TestSingleton>::acquire_instance_mutex();
        EXPECT_TRUE(lock1.owns_lock());
        lock1.unlock();
        EXPECT_FALSE(lock1.owns_lock());
    }
    {
        auto lock2 = Singleton<TestSingleton>::acquire_instance_mutex();
        EXPECT_TRUE(lock2.owns_lock());
    }
}

TEST_F(SingletonTest, ConcurrentHasInstanceCallsAreThreadSafe) {
    constexpr int kThreadCount = 8;
    constexpr int kIterations = 1000;
    std::atomic<bool> start{false};
    std::vector<std::thread> threads;

    threads.reserve(kThreadCount);

    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int j = 0; j < kIterations; ++j) {
                volatile bool has = Singleton<TestSingleton>::has_instance();
                (void)has;
            }
        });
    }

    start.store(true, std::memory_order_release);

    for (auto& thread : threads) {
        thread.join();
    }

    // No crash means thread-safe
    SUCCEED();
}

TEST_F(SingletonTest, GetInstanceIsIdempotent) {
    for (int i = 0; i < 100; ++i) {
        auto& instance = Singleton<TestSingleton>::get_instance();
        EXPECT_EQ(1, instance.get_constructed_count());
    }
    EXPECT_EQ(1, TestSingleton::get_global_construct_count());
}

// Test singleton with complex type
struct ComplexSingleton {
    std::vector<int> data;
    std::string name;
    double value;

    ComplexSingleton() : data{1, 2, 3, 4, 5}, name("test"), value(3.14) {}

    bool validate() const {
        return data.size() == 5 && name == "test" && value == 3.14;
    }
};

TEST(SingletonComplexTypeTest, WorksWithComplexTypes) {
    auto& instance = Singleton<ComplexSingleton>::get_instance();
    EXPECT_TRUE(instance.validate());

    auto& instance2 = Singleton<ComplexSingleton>::get_instance();
    EXPECT_EQ(&instance, &instance2);
}