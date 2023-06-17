#ifndef HSQR_RWMUTEX_DEADLOCK_DETECTOR_H_
#define HSQR_RWMUTEX_DEADLOCK_DETECTOR_H_

#pragma once

#include <unordered_map>
namespace hsqr {

class RWMutexDeadLockDetector {
public:
    RWMutexDeadLockDetector(void* id)
        : m_id(id)
    {
        auto& s = store();
        s.insert({ id, {} });
    }
    ~RWMutexDeadLockDetector()
    {
        auto& s = store();
        s.erase(m_id);
    }
    RWMutexDeadLockDetector(const RWMutexDeadLockDetector&) = delete;
    RWMutexDeadLockDetector& operator=(const RWMutexDeadLockDetector&) = delete;
    RWMutexDeadLockDetector(RWMutexDeadLockDetector&&) = delete;
    RWMutexDeadLockDetector& operator=(RWMutexDeadLockDetector&&) = delete;

    void read_locked()
    {
        auto& counter = store()[m_id];
        counter.reads += 1;
    }
    void read_unlocked()
    {
        auto& counter = store()[m_id];
        counter.reads -= 1;
    }
    void write_locked()
    {
        auto& counter = store()[m_id];
        counter.write = 1;
    }
    void write_unlocked()
    {
        auto& counter = store()[m_id];
        counter.write = 0;
    }
    bool can_read_lock()
    {
        auto& counter = store()[m_id];
        return counter.write == 0;
    }
    bool can_write_lock()
    {
        auto& counter = store()[m_id];
        return counter.write == 0 && counter.reads == 0;
    }

private:
    struct Counter {
        uint16_t reads = 0;
        uint16_t write = 0;
    };
    std::unordered_map<void*, Counter>& store()
    {
        thread_local static std::unordered_map<void*, Counter> s;
        return s;
    }
    void* m_id;
};

} // namespace

#endif