#ifndef HSQR_RWMUTEX_H_
#define HSQR_RWMUTEX_H_

#pragma once

#include <atomic>
#include <cassert>
#include <mutex>
#include <thread>

#include "hsqr/rwmutex-deadlock-detector.h"

namespace hsqr {

namespace test {
    struct RWMutexDiag;
};

template <typename DeadLockDetector_T>
class RWMutexImpl {
    friend struct hsqr::test::RWMutexDiag;

public:
    RWMutexImpl() noexcept
        : m_deadlockDetector(this)
    {
    }
    ~RWMutexImpl() noexcept
    {
        auto counter = m_counter.load();
        assert(counter.reads == 0);
        assert(counter.write == Counter::WriteNone);
    }

    RWMutexImpl(const RWMutexImpl&) = delete;
    RWMutexImpl& operator=(const RWMutexImpl&) = delete;
    RWMutexImpl(RWMutexImpl&&) = delete;
    RWMutexImpl& operator=(RWMutexImpl&&) = delete;

    // wait if has a writer. then increment the read counter and return
    void read_lock()
    {
        if (m_deadlockDetector.can_read_lock() == false) {
            throw std::logic_error(
                "Not allowed to mix read and write locks on the same thread");
        }

        while (true) {
            auto counter = m_counter.load();
            if (counter.write != Counter::WriteNone) {
                // wait for lock
                std::lock_guard<std::mutex> lock(m_mutex);
            } else {
                if (m_counter.compare_exchange_weak(
                        counter, Counter { counter.reads + 1, Counter::WriteNone })) {
                    // done
                    m_deadlockDetector.read_locked();
                    break;
                }
                // we have to re-try
            }
        }
    }
    // decrement the read counter
    void read_unlock()
    {
        while (true) {
            auto counter = m_counter.load();
            if (counter.reads == 0) {
                throw std::logic_error("Invalid call to unlock");
            }
            if (m_counter.compare_exchange_weak(
                    counter, Counter { counter.reads - 1, counter.write })) {
                m_deadlockDetector.read_unlocked();
                break;
            }
        }
    }
    // wait if already has a writer or has one or more readers. then lock mutex
    // and set the writer flag to true
    void write_lock()
    {
        if (m_deadlockDetector.can_write_lock() == false) {
            throw std::logic_error(
                "Not allowed to mix read and write locks on the same thread");
        }

        // first lock the mutex
        m_mutex.lock();
        // second set the write counter
        while (true) {
            auto counter = m_counter.load();
            if (m_counter.compare_exchange_weak(
                    counter, Counter { counter.reads, Counter::WriteWaiting })) {
                m_deadlockDetector.write_locked();
                break;
            }
        }
        // then wait for reads to go to zero
        while (true) {
            auto expected = Counter(0, Counter::WriteWaiting);
            if (m_counter.compare_exchange_weak(expected,
                    Counter(0, Counter::WriteOwned))) {
                break;
            }
            // spin lock (this could changed to a cv)
            std::this_thread::yield();
        }
    }
    // set the writer flag to false then unlock mutex
    void write_unlock()
    {
        auto expected = Counter(0, Counter::WriteOwned);
        if (!m_counter.compare_exchange_strong(expected,
                Counter(0, Counter::WriteNone))) {
            throw std::logic_error("Invalid call to unlock");
        }
        m_deadlockDetector.write_unlocked();
        m_mutex.unlock();
    }

private:
    struct Counter {
        // use int 16 to make the struct an atomic lock free
        enum WriteState : int16_t { WriteNone,
            WriteWaiting,
            WriteOwned };
        Counter() noexcept
            : reads(0)
            , write(WriteNone)
        {
        }
        Counter(int r, WriteState w)
            : reads(r)
            , write(w)
        {
        }
        uint16_t reads;
        WriteState write;
    };
    std::atomic<Counter> m_counter;
    std::mutex m_mutex;
    DeadLockDetector_T m_deadlockDetector;
};

class RWMutexNullDeadLockDetector {
public:
    RWMutexNullDeadLockDetector(void* id) { }
    void read_locked() { }
    void read_unlocked() { }
    void write_locked() { }
    void write_unlocked() { }
    bool can_read_lock() { return true; }
    bool can_write_lock() { return true; }
};

using RWMutexUnchecked = RWMutexImpl<RWMutexNullDeadLockDetector>;
using RWMutexChecked = RWMutexImpl<RWMutexDeadLockDetector>;

#ifdef NDEBUG
using RWMutex = RWMutexChecked;
#else
using RWMutex = RWMutexUnchecked;
#endif

} // namespace hsqr

#endif // HSQR_RWMUTEX_H_