#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <vector>

/// @brief SPSC Ring Buffer (capacity set at construction, never resized)
/// @tparam T : Type
template <typename T>
class RingBuffer
{
    std::vector<T> buffer_;
    std::atomic<size_t> head{0}, tail{0};
    size_t capacity_;

public:
    explicit RingBuffer(size_t capacity) : buffer_(capacity), capacity_(capacity)
    {
        assert(capacity > 1 && "RingBuffer capacity must be greater than one.");
    }

    bool push(const T &item)
    {
        auto t = tail.load(std::memory_order_relaxed);
        auto h = head.load(std::memory_order_acquire);
        if ((t + 1) % capacity_ == h)
            return false; // full
        buffer_[t] = item;
        tail.store((t + 1) % capacity_, std::memory_order_release);
        return true;
    }
    bool push(T &&item)
    {
        auto t = tail.load(std::memory_order_relaxed);
        auto h = head.load(std::memory_order_acquire);
        if ((t + 1) % capacity_ == h)
            return false; // full
        buffer_[t] = std::move(item);
        tail.store((t + 1) % capacity_, std::memory_order_release);
        return true;
    }
    bool pop(T &out)
    {
        auto h = head.load(std::memory_order_relaxed);
        auto t = tail.load(std::memory_order_acquire);
        if (h == t)
            return false; // empty
        out = buffer_[h];
        head.store((h + 1) % capacity_, std::memory_order_release);
        return true;
    }
    size_t Length() const noexcept
    {
        size_t h = head.load(std::memory_order_acquire);
        size_t t = tail.load(std::memory_order_acquire);

        if (t >= h)
        {
            return t - h;
        }
        else
        {
            // wrapped around
            return (capacity_ - h) + t;
        }
    }
};
