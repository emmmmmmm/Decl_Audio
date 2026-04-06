#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>

#include "../src/core/RingBuffer.hpp"

namespace
{
    bool Expect(bool condition, const char *message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            return false;
        }

        return true;
    }

    bool TestSingleThreadedSequence()
    {
        RingBuffer<int> queue(8);
        int value = -1;

        for (int i = 0; i < 7; ++i)
        {
            if (!Expect(queue.push(i), "push should succeed until queue reaches capacity"))
            {
                return false;
            }
        }

        if (!Expect(!queue.push(7), "push should fail once queue is full"))
        {
            return false;
        }

        if (!Expect(queue.Length() == 7, "Length should report the queued item count"))
        {
            return false;
        }

        for (int expected = 0; expected < 7; ++expected)
        {
            if (!Expect(queue.pop(value), "pop should succeed while items remain"))
            {
                return false;
            }

            if (!Expect(value == expected, "queue should preserve FIFO order"))
            {
                return false;
            }
        }

        if (!Expect(!queue.pop(value), "pop should fail after the queue is drained"))
        {
            return false;
        }

        return true;
    }

    bool TestWrapAround()
    {
        RingBuffer<int> queue(4);
        int value = -1;

        if (!Expect(queue.push(10), "initial push should succeed"))
        {
            return false;
        }

        if (!Expect(queue.push(20), "second push should succeed"))
        {
            return false;
        }

        if (!Expect(queue.pop(value) && value == 10, "first pop should read the oldest value"))
        {
            return false;
        }

        if (!Expect(queue.push(30), "push should succeed after advancing the head"))
        {
            return false;
        }

        if (!Expect(queue.push(40), "wrapped push should succeed"))
        {
            return false;
        }

        if (!Expect(!queue.push(50), "queue should still report full after wrapping"))
        {
            return false;
        }

        if (!Expect(queue.pop(value) && value == 20, "wrapped queue should keep FIFO order"))
        {
            return false;
        }

        if (!Expect(queue.pop(value) && value == 30, "wrapped queue should return the next item"))
        {
            return false;
        }

        if (!Expect(queue.pop(value) && value == 40, "wrapped queue should return the newest item last"))
        {
            return false;
        }

        return Expect(queue.Length() == 0, "Length should be zero after wrap-around drain");
    }

    bool TestConcurrentTransfer()
    {
        constexpr int kItemCount = 200000;

        RingBuffer<int> queue(1024);
        std::atomic<bool> producer_done{false};
        std::atomic<bool> failed{false};

        std::thread producer([&]()
                             {
        for (int i = 0; i < kItemCount; ++i)
        {
            while (!queue.push(i))
            {
                std::this_thread::yield();
            }
        }

        producer_done.store(true, std::memory_order_release); });

        std::thread consumer([&]()
                             {
        int expected = 0;
        int value = -1;

        while (expected < kItemCount)
        {
            if (queue.pop(value))
            {
                if (value != expected)
                {
                    failed.store(true, std::memory_order_release);
                    return;
                }

                ++expected;
                continue;
            }

            if (producer_done.load(std::memory_order_acquire))
            {
                failed.store(true, std::memory_order_release);
                return;
            }

            std::this_thread::yield();
        } });

        producer.join();
        consumer.join();

        if (!Expect(!failed.load(std::memory_order_acquire), "concurrent transfer should preserve item order"))
        {
            return false;
        }

        return Expect(queue.Length() == 0, "queue should be empty after producer/consumer completion");
    }
} // namespace

bool RunRingBufferTests()
{
    if (!TestSingleThreadedSequence())
    {
        return false;
    }

    if (!TestWrapAround())
    {
        return false;
    }

    if (!TestConcurrentTransfer())
    {
        return false;
    }

    std::cout << "RingBuffer tests passed\n";
    return true;
}
