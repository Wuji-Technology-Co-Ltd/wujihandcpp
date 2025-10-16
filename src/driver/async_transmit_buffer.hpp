#pragma once

#include <chrono>

#include <libusb.h>

#include <wujihandcpp/utility/logger.hpp>

#include "driver/driver.hpp"
#include "utility/cross_os.hpp"
#include "utility/ring_buffer.hpp"

namespace wujihandcpp::driver {

template <typename Device>
template <typename TransferPrefill>
class Driver<Device>::AsyncTransmitBuffer final {
public:
    static_assert(is_legal_transfer_prefill<TransferPrefill>);

    explicit AsyncTransmitBuffer(Driver& driver, size_t alloc_transfer_count)
        : driver_(driver)
        , free_transfers_(alloc_transfer_count)
        , alloc_transfer_count_(alloc_transfer_count) {

        free_transfers_.push_back_multi(
            [this]() {
                auto transfer = libusb_alloc_transfer(0);
                if (!transfer)
                    throw std::bad_alloc{};

                libusb_fill_bulk_transfer(
                    transfer, driver_.libusb_device_handle_, Driver::out_endpoint_,
                    new unsigned char[max_transmit_length_], prefill_size_,
                    [](libusb_transfer* transfer) {
                        static_cast<AsyncTransmitBuffer*>(transfer->user_data)
                            ->usb_transmit_complete_callback(transfer);
                    },
                    this, 0);
                transfer->flags = libusb_transfer_flags::LIBUSB_TRANSFER_FREE_BUFFER;
                if constexpr (!std::is_same_v<TransferPrefill, void>)
                    new (&transfer->buffer[0]) TransferPrefill{};

                return transfer;
            },
            alloc_transfer_count_);
    }

    /**
     * @brief Releases all allocated libusb transfer resources and ensures outstanding transfers complete.
     *
     * Attempts to free every pre-allocated transfer, waiting briefly for outstanding asynchronous
     * callbacks to return transfers to the free pool. If transfers cannot be reclaimed within
     * a short timeout, logs fatal errors describing the failure and the remaining leaked transfer count.
     */
    ~AsyncTransmitBuffer() {
        size_t unreleased_transfer_count = alloc_transfer_count_;
        timeval timeout{1, 0};
        auto start = std::chrono::steady_clock::now();
        while (true) {
            unreleased_transfer_count -= free_transfers_.pop_front_multi(
                [&](libusb_transfer* transfer) { libusb_free_transfer(transfer); });

            // Break when all transfer released
            if (!unreleased_transfer_count)
                break;

            int ret;
            // Otherwise, handle events to allow other transfers to return to the queue
            if constexpr (utility::is_linux()) {
                // Set a 1s timeout to avoid stuck here (logically impossible, but just in case)
                ret = libusb_handle_events_timeout(driver_.libusb_context_, &timeout);
            } else {
                // Windows does not support timeout
                ret = libusb_handle_events(driver_.libusb_context_);
            }
            if (ret != 0) {
                WUJI_ERROR(
                    "Fatal error during TransmitBuffer destruction: The function "
                    "libusb_handle_events returned an exception value: {}, which means we "
                    "cannot release all memory allocated for transfers.", static_cast<int>(ret));
            } else if (std::chrono::steady_clock::now() - start > std::chrono::seconds(1)) {
                WUJI_ERROR(
                    "Fatal error during TransmitBuffer destruction: The usb transmit complete "
                    "callback was not called for all transfers, which means we cannot release "
                    "all memory allocated for transfers.");
            } else
                continue;

            WUJI_ERROR(
                "The destructor will exit normally, but the unrecoverable memory leak "
                "has already occurred. This may be a problem caused by libusb.");
            WUJI_ERROR("Number of leaked transfers: {}", static_cast<size_t>(unreleased_transfer_count));
            break;
        }
    }

    std::byte* try_fetch_buffer(int size) {
        return try_fetch_buffer(
            [&size](int free_size, libusb_transfer*) { return free_size >= size; },
            [&size](int) { return size; });
    }

    template <typename F1, typename F2>
    requires requires(const F1& f, int free_size, libusb_transfer* transfer) {
        { f(free_size, transfer) } -> std::convertible_to<bool>;
    } && requires(const F2& f, int free_size) {
        { f(free_size) } -> std::convertible_to<int>;
    } /**
     * Acquire a writable buffer region from a preallocated transmit transfer that meets the provided acceptance predicate.
     *
     * @tparam F1 Type of the acceptance predicate callable.
     * @tparam F2 Type of the size selection callable.
     * @param check_transfer Callable invoked as `check_transfer(free_size, transfer)` to decide whether the current transfer is acceptable; should return `true` to accept the transfer, `false` to reject it (causes the buffer to be considered for submission).
     * @param get_actual_size Callable invoked as `get_actual_size(free_size)` to determine how many bytes to reserve from the accepted transfer's remaining free space; its return value must be <= `free_size`.
     * @return std::byte* Pointer to the start of the reserved buffer region within the transfer when successful, or `nullptr` if no suitable buffer is available.
     */
    std::byte* try_fetch_buffer(const F1& check_transfer, const F2& get_actual_size) {
        while (true) {
            auto front = free_transfers_.front();
            if (!front) [[unlikely]] {
                if (!transfers_all_busy_)
                    WUJI_ERROR("Failed to fetch free buffer: All transfers are busy!");
                transfers_all_busy_ = true;
                return nullptr;
            } else
                transfers_all_busy_ = false;

            libusb_transfer* transfer = *front;

            auto free_size = max_transmit_length_ - transfer->length;
            if (!check_transfer(free_size, transfer))
                trigger_transmission_nocheck();
            else {
                int size = get_actual_size(free_size);
                if (free_size < size) [[unlikely]]
                    return nullptr;
                std::byte* buffer =
                    reinterpret_cast<std::byte*>(transfer->buffer) + transfer->length;
                transfer->length += static_cast<int>(size);
                return buffer;
            }
        }
    }

    bool trigger_transmission() {
        auto front = free_transfers_.front();
        if (!front || (*front)->length <= prefill_size_)
            return false;

        return trigger_transmission_nocheck();
    }

private:
    /**
     * @brief Attempts to submit a prepared transfer from the free pool to libusb.
     *
     * Pops a pre-allocated transmit transfer from the internal free pool, invokes the device
     * hook before submission, and submits the transfer to libusb for transmission.
     *
     * @returns true if a transfer was popped and successfully submitted, false if no free transfer was available.
     *
     * @note Calls Device::before_submitting_transmit_transfer(transfer) prior to submission.
     * @note If libusb_submit_transfer fails the function logs an error and terminates the process via std::terminate().
     */
    bool trigger_transmission_nocheck() {
        libusb_transfer* transfer = nullptr;

        if (!free_transfers_.pop_front([&transfer](libusb_transfer* t) { transfer = t; }))
            return false;
        // The transfer must be submitted to libusb only after the pop_front function returns.
        // Otherwise, there is a very slight chance that the callback might be invoked too
        // quickly, resulting in a false "ring queue full" condition when recycling transfer,
        // which could subsequently lead to transfer leaks.

        static_cast<Device&>(driver_).before_submitting_transmit_transfer(transfer);

        int ret = libusb_submit_transfer(transfer);
        if (ret != 0) [[unlikely]] {
            if (ret == LIBUSB_ERROR_NO_DEVICE)
                WUJI_ERROR(
                    "Failed to submit transmit transfer: Device disconnected. Terminating...");
            else
                WUJI_ERROR("Failed to submit transmit transfer: {}. Terminating...", static_cast<int>(ret));
            std::terminate();
        }

        return true;
    }

    /**
     * @brief Handle a completed asynchronous USB transmit transfer and recycle it.
     *
     * Resets the transfer's length to the configured prefill size, invokes the device's
     * transmit completion callback, and attempts to push the transfer back into the free
     * pool. Logs errors when the transfer status is not completed or when the actual
     * transmitted length differs from the expected length. If recycling into the pool
     * fails, the process is terminated.
     *
     * @param transfer Pointer to the completed libusb_transfer.
     */
    void usb_transmit_complete_callback(libusb_transfer* transfer) {
        if (transfer->status != LIBUSB_TRANSFER_COMPLETED) [[unlikely]] {
            WUJI_ERROR(
                "USB transmitting error: Transfer not completed! status={}", static_cast<int>(transfer->status));
        }

        if (transfer->actual_length != transfer->length) [[unlikely]]
            WUJI_ERROR(
                "USB transmitting error: transmitted({}) < expected({})",
                static_cast<int>(transfer->actual_length), static_cast<int>(transfer->length));

        transfer->length = prefill_size_;

        static_cast<Device&>(driver_).transmit_transfer_completed_callback(transfer);

        if (!free_transfers_.push_back(transfer)) [[unlikely]] {
            WUJI_ERROR(
                "Error while attempting to recycle transmit transfer into the ring queue: "
                "The ring queue is full.");
            WUJI_ERROR(
                "This situation should theoretically be impossible. Its occurrence typically "
                "indicates an issue with multithreaded synchronization in the code.");
            WUJI_ERROR(
                "Although this problem is not fatal, termination is triggered to ensure the "
                "issue is promptly identified.");
            std::terminate();
        }
    }

    static constexpr int prefill_size_ = []() {
        if constexpr (std::is_same_v<TransferPrefill, void>)
            return 0;
        else
            return int(sizeof(TransferPrefill));
    }();

    Driver& driver_;

    utility::RingBuffer<libusb_transfer*> free_transfers_;
    size_t alloc_transfer_count_;

    bool transfers_all_busy_ = false;
};

} // namespace wujihandcpp::driver