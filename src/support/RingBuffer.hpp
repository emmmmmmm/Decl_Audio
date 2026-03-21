#pragma once

#include <memory>
#include <atomic>

template<typename T, size_t N>
class RingBuffer {
    std::atomic<size_t> head{ 0 }, tail{ 0 };
    T buffer[N];
public:
    bool push(const T& item) {
        auto t = tail.load(std::memory_order_relaxed);
        auto h = head.load(std::memory_order_acquire);
        if ((t + 1) % N == h) return false; // full
        buffer[t] = item;
        tail.store((t + 1) % N, std::memory_order_release);
        return true;
    }
    bool pop(T& out) {
        auto h = head.load(std::memory_order_relaxed);
        auto t = tail.load(std::memory_order_acquire);
        if (h == t) return false; // empty
        out = buffer[h];
        head.store((h + 1) % N, std::memory_order_release);
        return true;
    }
    size_t Length() const noexcept {
        // grab both atomically
        size_t h = head.load(std::memory_order_acquire);
        size_t t = tail.load(std::memory_order_acquire);

        if (t >= h) {
            return t - h;
        }
        else {
            // wrapped around
            return (N - h) + t;
        }
    }
};
