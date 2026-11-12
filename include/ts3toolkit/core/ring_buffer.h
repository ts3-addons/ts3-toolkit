#pragma once

#include "ts3toolkit/core/types.h"

#include <atomic>
#include <cstddef>
#include <vector>

namespace ts3toolkit::core {

// Bounded single-producer/single-consumer ring buffer of PCM samples.
//
// At most one thread writes and one thread reads concurrently. The capacity is
// rounded up to a power of two so index wrapping is a bit mask. All operations
// are non-blocking and return partial counts instead of overwriting unread
// data or blocking.
class RingBuffer {
public:
    explicit RingBuffer(std::size_t requested_capacity);

    // Enqueue up to count samples; returns the number written, which may be
    // smaller than count when free space is insufficient.
    std::size_t write(const Sample* src, std::size_t count) noexcept;

    // Dequeue up to count samples; returns the number read.
    std::size_t read(Sample* dst, std::size_t count) noexcept;

    std::size_t size() const noexcept;
    std::size_t capacity() const noexcept;
    bool isEmpty() const noexcept;
    bool isFull() const noexcept;

    // Drop all buffered samples; not safe to call concurrently with a reader.
    void reset() noexcept;

private:
    static std::size_t nextPowerOfTwo(std::size_t value) noexcept;

    std::vector<Sample> storage_;
    std::size_t mask_;
    std::atomic<std::size_t> head_;  // read position, owned by the reader
    std::atomic<std::size_t> tail_;  // write position, owned by the writer
};

}  // namespace ts3toolkit::core
