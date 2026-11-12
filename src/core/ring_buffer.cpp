#include "ts3toolkit/core/ring_buffer.h"

#include <algorithm>

namespace ts3toolkit::core {

namespace {
constexpr std::size_t kMinCapacity = 2;
}  // namespace

std::size_t RingBuffer::nextPowerOfTwo(std::size_t value) noexcept {
    std::size_t power = kMinCapacity;
    while (power < value) {
        power <<= 1;
    }
    return power;
}

RingBuffer::RingBuffer(std::size_t requested_capacity)
    : storage_(nextPowerOfTwo(requested_capacity), Sample{0}),
      mask_(storage_.size() - 1),
      head_(0),
      tail_(0) {}

std::size_t RingBuffer::write(const Sample* src,
                               std::size_t count) noexcept {
    const std::size_t head = head_.load(std::memory_order_acquire);
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    const std::size_t free_space = storage_.size() - (tail - head);
    const std::size_t to_write = (std::min)(count, free_space);

    for (std::size_t i = 0; i < to_write; ++i) {
        storage_[(tail + i) & mask_] = src[i];
    }
    tail_.store(tail + to_write, std::memory_order_release);
    return to_write;
}

std::size_t RingBuffer::read(Sample* dst, std::size_t count) noexcept {
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t available = tail - head;
    const std::size_t to_read = (std::min)(count, available);

    for (std::size_t i = 0; i < to_read; ++i) {
        dst[i] = storage_[(head + i) & mask_];
    }
    head_.store(head + to_read, std::memory_order_release);
    return to_read;
}

std::size_t RingBuffer::size() const noexcept {
    return tail_.load(std::memory_order_acquire) -
           head_.load(std::memory_order_acquire);
}

std::size_t RingBuffer::capacity() const noexcept {
    return storage_.size();
}

bool RingBuffer::isEmpty() const noexcept {
    return size() == 0;
}

bool RingBuffer::isFull() const noexcept {
    return size() == storage_.size();
}

void RingBuffer::reset() noexcept {
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
}

}  // namespace ts3toolkit::core
