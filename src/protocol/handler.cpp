#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <map>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>

#include <wujihandcpp/protocol/handler.hpp>

#include "driver/async_transmit_buffer.hpp"
#include "driver/driver.hpp"
#include "protocol/protocol.hpp"

namespace wujihandcpp::protocol {

class Handler::Impl : driver::Driver<Impl> {
    friend class Driver<Impl>;
    friend class AsyncTransmitBuffer<protocol::Header>;

public:
    explicit Impl(
        uint16_t usb_vid, int32_t usb_pid, const char* serial_number, size_t buffer_transfer_count,
        size_t storage_unit_count)
        : Driver(usb_vid, usb_pid, serial_number)
        , default_transmit_buffer_(*this, buffer_transfer_count)
        , tick_thread_transmit_buffer_(*this, buffer_transfer_count)
        , event_thread_([this]() { handle_events(); })
        , operation_thread_id_(std::this_thread::get_id())
        , storage_unit_count_(storage_unit_count)
        , storage_(std::make_unique<StorageUnit[]>(storage_unit_count))
        , tick_thread_(
              [this](const std::stop_token& stop_token) { tick_thread_main(stop_token); }) {}

    ~Impl() { stop_handling_events(); };

    void init_storage_info(int storage_id, StorageInfo info) {
        // std::cout << std::format(
        //     "[{:3}]: 0x{:02X}, {:2} ({}) = {:04X}\n", storage_id, (int)info.index,
        //     (int)info.sub_index,
        //     info.size == StorageInfo::Size::_1 ? 1
        //                                        : (info.size == StorageInfo::Size::_2   ? 2
        //                                           : info.size == StorageInfo::Size::_4 ? 4
        //                                                                                : 8),
        //     (int)info.policy);
        storage_[storage_id].info = info;
        IndexMapKey index{.index = info.index, .sub_index = info.sub_index};
        index_storage_map_[std::bit_cast<uint32_t>(index)] = &storage_[storage_id];
    }

    void read_async_unchecked(int storage_id) {
        operation_thread_check();

        if (storage_[storage_id].operation.load(std::memory_order::relaxed).mode
            != Operation::Mode::NONE)
            return;

        storage_[storage_id].callback = nullptr;
        storage_[storage_id].operation.store(
            Operation{.mode = Operation::Mode::READ, .state = Operation::State::READING},
            std::memory_order::release);
    }

    void read_async(
        int storage_id, void (*callback)(Buffer8 context, Buffer8 value),
        Buffer8 callback_context) {
        operation_thread_check();

        if (storage_[storage_id].operation.load(std::memory_order::relaxed).mode
            != Operation::Mode::NONE) [[unlikely]]
            throw std::runtime_error("Illegal checked read: Data is being operated!");

        storage_[storage_id].callback = callback;
        storage_[storage_id].callback_context = callback_context;
        storage_[storage_id].operation.store(
            Operation{.mode = Operation::Mode::READ, .state = Operation::State::READING},
            std::memory_order::release);
    }

    void write_async_unchecked(Buffer8 data, int storage_id) {
        operation_thread_check();

        store_data(storage_[storage_id], data);

        if (storage_[storage_id].operation.load(std::memory_order::relaxed).mode
            != Operation::Mode::NONE)
            return;

        storage_[storage_id].callback = nullptr;
        storage_[storage_id].operation.store(
            Operation{.mode = Operation::Mode::WRITE, .state = Operation::State::WRITING},
            std::memory_order::release);
    }

    void write_async(
        Buffer8 data, int storage_id, void (*callback)(Buffer8 context, Buffer8 value),
        Buffer8 callback_context) {
        operation_thread_check();

        if (storage_[storage_id].operation.load(std::memory_order::relaxed).mode
            != Operation::Mode::NONE) [[unlikely]]
            throw std::runtime_error("Illegal checked write: Data is being operated!");

        store_data(storage_[storage_id], data);
        storage_[storage_id].callback = callback;
        storage_[storage_id].callback_context = callback_context;
        storage_[storage_id].operation.store(
            Operation{.mode = Operation::Mode::WRITE, .state = Operation::State::WRITING},
            std::memory_order::release);
    }

    void pdo_write_async_unchecked(const double (&control_positions)[5][4], uint32_t timestamp) {
        operation_thread_check();

        std::byte* buffer = fetch_pdo_buffer(default_transmit_buffer_);
        auto payload = new (buffer) protocol::pdo::Write{};

        payload->pdo_id = 0x100;
        for (int i = 0; i < 5; i++)
            for (int j = 0; j < 4; j++) {
                payload->control_positions[i][j] = to_raw_position(control_positions[i][j]);
                if (j == 0 && i != 0)
                    payload->control_positions[i][j] = -payload->control_positions[i][j];
            }
        payload->timestamp = timestamp;

        trigger_transmission();
    }

    bool trigger_transmission() {
        operation_thread_check();
        return default_transmit_buffer_.trigger_transmission();
    }

    Buffer8 get(int storage_id) { return load_data(storage_[storage_id]); }

    void disable_thread_safe_check() { operation_thread_id_ = std::thread::id{}; }

private:
    struct Operation {
        enum class Mode : uint16_t {
            NONE = 0,

            READ,
            WRITE
        } mode;
        enum class State : uint16_t {
            SUCCESS = 0,

            READING,

            WRITING,
            WRITING_CONFIRMING,
        } state;
    };
    struct StorageUnit {
        StorageInfo info;

        std::atomic<Operation> operation =
            Operation{.mode = Operation::Mode::NONE, .state = Operation::State::SUCCESS};

        std::atomic<uint32_t> version = 0;
        std::atomic<Buffer8> value;

        void (*callback)(Buffer8 context, Buffer8 value);
        Buffer8 callback_context;
    };
    static_assert(sizeof(StorageUnit) == 40);
    static_assert(decltype(StorageUnit::operation)::is_always_lock_free);
    static_assert(decltype(StorageUnit::version)::is_always_lock_free);
    static_assert(decltype(StorageUnit::value)::is_always_lock_free);

    void operation_thread_check() const {
        if (operation_thread_id_ == std::thread::id{})
            return;
        if (operation_thread_id_ != std::this_thread::get_id()) [[unlikely]]
            throw std::runtime_error(
                "Thread safety violation: \n"
                "  Operation must be called from the construction thread by default. \n"
                "  If you want to perform operations in multiple threads, call:\n"
                "      disable_thread_safe_check();\n"
                "  And use mutex to ensure that ONLY ONE THREAD is operating at the same time.");
    }

    static void store_data(StorageUnit& storage, Buffer8 data) {
        if (storage.info.policy & StorageInfo::POSITION_FLOATING) {
            auto value = to_raw_position(data.as<double>());
            if (storage.info.policy & StorageInfo::POSITION_REVERSED)
                value = -value;
            storage.value.store(Buffer8{value}, std::memory_order::relaxed);
        } else
            storage.value.store(data, std::memory_order::relaxed);
    }

    static constexpr int32_t to_raw_position(double angle) {
        return static_cast<int32_t>(std::round(
            std::clamp<double>(
                angle * (std::numeric_limits<int32_t>::max() / (2 * std::numbers::pi)),
                std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max())));
    }

    static Buffer8 load_data(StorageUnit& storage) {
        Buffer8 data = storage.value.load(std::memory_order::relaxed);

        if (storage.info.policy & StorageInfo::POSITION_FLOATING) {
            auto value =
                data.as<int32_t>() * (2 * std::numbers::pi / std::numeric_limits<int32_t>::max());
            if (storage.info.policy & StorageInfo::POSITION_REVERSED)
                value = -value;
            return Buffer8{value};
        }

        return data;
    }

    static std::byte*
        fetch_sdo_buffer(AsyncTransmitBuffer<protocol::Header>& transmit_buffer, int size) {
        auto buffer = transmit_buffer.try_fetch_buffer(
            [size](int free_size, libusb_transfer* transfer) {
                if (free_size < size + int(sizeof(protocol::CrcCheck)))
                    return false;

                auto& header = *reinterpret_cast<protocol::Header*>(transfer->buffer);
                if (header.type == 0)
                    header.type = 0x21;
                else if (header.type != 0x21)
                    return false;

                return true;
            },
            [size](int) { return size; });
        if (!buffer)
            throw std::runtime_error{"No buffer available!"};

        return buffer;
    }

    static std::byte* fetch_pdo_buffer(AsyncTransmitBuffer<protocol::Header>& transmit_buffer) {
        auto buffer = transmit_buffer.try_fetch_buffer(
            [](int free_size, libusb_transfer* transfer) {
                if (free_size < int(sizeof(protocol::pdo::Write) + sizeof(protocol::CrcCheck)))
                    return false;

                auto& header = *reinterpret_cast<protocol::Header*>(transfer->buffer);
                if (header.type == 0)
                    header.type = 0x11;
                else if (header.type != 0x11)
                    return false;

                return true;
            },
            [](int) { return sizeof(protocol::pdo::Write); });
        if (!buffer)
            throw std::runtime_error{"No buffer available!"};

        return buffer;
    }

    static void before_submitting_transmit_transfer(libusb_transfer* transfer) {
        auto compressed_frame_length = static_cast<uint16_t>(
            (transfer->length + (int)sizeof(protocol::CrcCheck) - 1) / 16 + 1);
        auto padded_length = 16 * compressed_frame_length;
        std::memset(&transfer->buffer[transfer->length], 0, padded_length - transfer->length);
        transfer->length = padded_length;

        auto& header = *reinterpret_cast<protocol::Header*>(transfer->buffer);
        struct {
            uint16_t max_receive_window : 10;
            uint16_t frame_length       : 6;
        } description{
            .max_receive_window = 0xA0, .frame_length = (uint8_t)(compressed_frame_length - 1)};
        header.description = std::bit_cast<int16_t>(description);
    }

    static void transmit_transfer_completed_callback(libusb_transfer* transfer) {
        // std::cout << std::format(
        //     "[{:.3f}] Tx: ", std::chrono::duration<double, std::milli>(
        //                          std::chrono::steady_clock::now().time_since_epoch())
        //                          .count());
        // for (int i = 0; i < transfer->actual_length; i++)
        //     std::cout << std::format("{:02X} ", transfer->buffer[i]);
        // std::cout << '\n';

        auto& header = *reinterpret_cast<protocol::Header*>(transfer->buffer);
        header.type = 0;
    }

    void receive_transfer_completed_callback(libusb_transfer* transfer) {
        // std::cout << std::format(
        //     "[{:.3f}] Rx: ", std::chrono::duration<double, std::milli>(
        //                          std::chrono::steady_clock::now().time_since_epoch())
        //                          .count());
        // for (int i = 0; i < transfer->actual_length; i++)
        //     std::cout << std::format("{:02X} ", transfer->buffer[i]);
        // std::cout << '\n';

        auto pointer = reinterpret_cast<std::byte*>(transfer->buffer);
        const auto sentinel = pointer + transfer->actual_length;

        auto& header = *reinterpret_cast<protocol::Header*>(pointer);
        pointer += sizeof(protocol::Header);

        if (header.type == 0x21)
            read_sdo_frame(pointer, sentinel);
    }

    void read_sdo_frame(std::byte*& pointer, const std::byte* sentinel) {
        while (pointer < sentinel) {
            auto control = static_cast<uint8_t>(*pointer);
            if (control == 0x35)
                read_sdo_operation_read_success<uint8_t>(pointer);
            else if (control == 0x37)
                read_sdo_operation_read_success<uint16_t>(pointer);
            else if (control == 0x39)
                read_sdo_operation_read_success<uint32_t>(pointer);
            else if (control == 0x3D)
                read_sdo_operation_read_success<uint64_t>(pointer);
            else if (control == 0x33)
                read_sdo_operation_read_failed(pointer);
            else if (control == 0x21)
                read_sdo_operation_write_success(pointer);
            else if (control == 0x23)
                read_sdo_operation_write_failed(pointer);
            else
                break;
        }
    }

    template <typename T>
    void read_sdo_operation_read_success(std::byte*& pointer) {
        const auto& data = *reinterpret_cast<protocol::sdo::ReadResultSuccess<T>*>(pointer);
        pointer += sizeof(data);

        StorageUnit& storage = find_storage_by_index(data.header.index, data.header.sub_index);

        auto operation = storage.operation.load(std::memory_order::acquire);
        if (operation.mode == Operation::Mode::NONE) [[unlikely]]
            return;

        if (operation.state == Operation::State::READING) {
            storage.value.store(Buffer8{data.value}, std::memory_order::relaxed);
            auto new_version = storage.version.load(std::memory_order::relaxed) + 1;
            if (new_version == 0)
                new_version = 1;
            storage.version.store(new_version, std::memory_order::release);

            operation.state = Operation::State::SUCCESS;
            storage.operation.store(operation, std::memory_order::release);
        } else if (operation.state == Operation::State::WRITING_CONFIRMING) {
            if (data.value == storage.value.load(std::memory_order::relaxed).as<T>()) {
                operation.state = Operation::State::SUCCESS;
                storage.operation.store(operation, std::memory_order::relaxed);
            } else {
                operation.state = Operation::State::WRITING;
                storage.operation.store(operation, std::memory_order::relaxed);
            }
        }
    }

    void read_sdo_operation_read_failed(std::byte*& pointer) {
        const auto& data = *reinterpret_cast<protocol::sdo::ReadResultError*>(pointer);
        pointer += sizeof(data);

        StorageUnit& storage = find_storage_by_index(data.header.index, data.header.sub_index);
        (void)storage;
    }

    void read_sdo_operation_write_success(std::byte*& pointer) {
        const auto& data = *reinterpret_cast<protocol::sdo::WriteResultSuccess*>(pointer);
        pointer += sizeof(data);

        StorageUnit& storage = find_storage_by_index(data.header.index, data.header.sub_index);

        auto operation = storage.operation.load(std::memory_order::acquire);
        if (operation.mode == Operation::Mode::NONE) [[unlikely]]
            return;

        if (operation.state == Operation::State::WRITING) {
            operation.state = Operation::State::SUCCESS;
            storage.operation.store(operation, std::memory_order::relaxed);
        }
    }

    void read_sdo_operation_write_failed(std::byte*& pointer) {
        const auto& data = *reinterpret_cast<protocol::sdo::WriteResultError*>(pointer);
        pointer += sizeof(data);

        StorageUnit& storage = find_storage_by_index(data.header.index, data.header.sub_index);

        auto operation = storage.operation.load(std::memory_order::acquire);
        if (operation.mode == Operation::Mode::NONE) [[unlikely]]
            return;

        if (operation.state == Operation::State::WRITING) {
            operation.state = Operation::State::WRITING_CONFIRMING;
            storage.operation.store(operation, std::memory_order::relaxed);
        }
    }

    StorageUnit& find_storage_by_index(uint16_t index, uint8_t sub_index) {
        auto it = index_storage_map_.find(
            std::bit_cast<uint32_t>(IndexMapKey{.index = index, .sub_index = sub_index}));
        if (it == index_storage_map_.end())
            throw std::runtime_error{"Unexpected sdo index & sub-index!"};
        return *it->second;
    }

    void tick_thread_main(const std::stop_token& token) {
        constexpr double update_rate = 199.0;
        constexpr auto update_period =
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(1.0 / update_rate));

        while (!token.stop_requested()) {
            for (size_t i = 0; i < storage_unit_count_; i++) {
                auto& storage = storage_[i];

                auto operation = storage.operation.load(std::memory_order::acquire);
                if (operation.mode == Operation::Mode::NONE)
                    continue;

                if (operation.state == Operation::State::SUCCESS) {
                    operation.mode = Operation::Mode::NONE;
                    storage.operation.store(operation, std::memory_order::relaxed);

                    if (storage.callback)
                        storage.callback(
                            storage.callback_context,
                            storage.value.load(std::memory_order::relaxed));
                } else if (
                    operation.state == Operation::State::READING
                    || operation.state == Operation::State::WRITING_CONFIRMING) {
                    read_async_unchecked_internal(
                        tick_thread_transmit_buffer_, storage.info.index, storage.info.sub_index);
                } else if (operation.state == Operation::State::WRITING) {
                    if (storage.info.size == StorageInfo::Size::_1)
                        write_async_unchecked_internal(
                            tick_thread_transmit_buffer_,
                            storage.value.load(std::memory_order::relaxed).as<uint8_t>(),
                            storage.info.index, storage.info.sub_index);
                    else if (storage.info.size == StorageInfo::Size::_2)
                        write_async_unchecked_internal(
                            tick_thread_transmit_buffer_,
                            storage.value.load(std::memory_order::relaxed).as<uint16_t>(),
                            storage.info.index, storage.info.sub_index);
                    else if (storage.info.size == StorageInfo::Size::_4)
                        write_async_unchecked_internal(
                            tick_thread_transmit_buffer_,
                            storage.value.load(std::memory_order::relaxed).as<uint32_t>(),
                            storage.info.index, storage.info.sub_index);
                    else if (storage.info.size == StorageInfo::Size::_8)
                        write_async_unchecked_internal(
                            tick_thread_transmit_buffer_,
                            storage.value.load(std::memory_order::relaxed).as<uint64_t>(),
                            storage.info.index, storage.info.sub_index);
                }
            }
            tick_thread_transmit_buffer_.trigger_transmission();

            std::this_thread::sleep_for(update_period);
        }
    }

    static void read_async_unchecked_internal(
        AsyncTransmitBuffer<protocol::Header>& transmit_buffer, uint16_t index, uint8_t sub_index) {
        std::byte* buffer = fetch_sdo_buffer(transmit_buffer, sizeof(protocol::sdo::Read));
        new (buffer) protocol::sdo::Read{
            .index = index,
            .sub_index = sub_index,
        };
    }

    template <protocol::is_type_erased_integral T>
    void write_async_unchecked_internal(
        AsyncTransmitBuffer<protocol::Header>& transmit_buffer, T value, uint16_t index,
        uint8_t sub_index) {

        std::byte* buffer = fetch_sdo_buffer(transmit_buffer, sizeof(protocol::sdo::Write<T>));
        new (buffer) protocol::sdo::Write<T>{
            .index = index,
            .sub_index = sub_index,
            .value = value,
        };
    }

    template <size_t size>
    using SizeToUIntType = std::conditional_t<
        size == 1, uint8_t,
        std::conditional_t<
            size == 2, uint16_t,
            std::conditional_t<
                size == 4, uint32_t, std::conditional_t<size == 8, uint64_t, void>>>>;

    AsyncTransmitBuffer<protocol::Header> default_transmit_buffer_;
    AsyncTransmitBuffer<protocol::Header> tick_thread_transmit_buffer_;
    std::jthread event_thread_;

    std::thread::id operation_thread_id_;

    size_t storage_unit_count_;
    std::unique_ptr<StorageUnit[]> storage_;

    struct IndexMapKey {
        uint16_t index;
        uint8_t sub_index;
        const uint8_t padding = 0;
    };
    std::map<uint32_t, StorageUnit*> index_storage_map_;

    std::jthread tick_thread_;
};

API Handler::Handler(
    uint16_t usb_vid, int32_t usb_pid, const char* serial_number, size_t buffer_transfer_count,
    size_t storage_unit_count) {
    static_assert(impl_align == alignof(Handler::Impl));
    static_assert(sizeof(impl_) == sizeof(Handler::Impl));
    new (impl_) Impl{usb_vid, usb_pid, serial_number, buffer_transfer_count, storage_unit_count};
}

API Handler::~Handler() { std::destroy_at(reinterpret_cast<Impl*>(impl_)); }

API void Handler::init_storage_info(int storage_id, StorageInfo info) {
    reinterpret_cast<Impl*>(impl_)->init_storage_info(storage_id, info);
}

API void Handler::read_async_unchecked(int storage_id) {
    reinterpret_cast<Impl*>(impl_)->read_async_unchecked(storage_id);
}

API void Handler::read_async(
    int storage_id, void (*callback)(Buffer8 context, Buffer8 value), Buffer8 callback_context) {
    reinterpret_cast<Impl*>(impl_)->read_async(storage_id, callback, callback_context);
}

API void Handler::write_async_unchecked(Buffer8 data, int storage_id) {
    reinterpret_cast<Impl*>(impl_)->write_async_unchecked(data, storage_id);
}

API void Handler::write_async(
    Buffer8 data, int storage_id, void (*callback)(Buffer8 context, Buffer8 value),
    Buffer8 callback_context) {
    reinterpret_cast<Impl*>(impl_)->write_async(data, storage_id, callback, callback_context);
}

API void Handler::pdo_write_async_unchecked(
    const double (&control_positions)[5][4], uint32_t timestamp) {
    reinterpret_cast<Impl*>(impl_)->pdo_write_async_unchecked(control_positions, timestamp);
}

API bool Handler::trigger_transmission() {
    return reinterpret_cast<Impl*>(impl_)->trigger_transmission();
}

API Handler::Buffer8 Handler::get(int storage_id) {
    return reinterpret_cast<Impl*>(impl_)->get(storage_id);
}

API void Handler::disable_thread_safe_check() {
    return reinterpret_cast<Impl*>(impl_)->disable_thread_safe_check();
}

} // namespace wujihandcpp::protocol