#ifndef HSQR_RWLOCK_H_
#define HSQR_RWLOCK_H_

#pragma once

#include "hsqr/rwmutex.h"
#include <type_traits>
#include <utility>

namespace hsqr {

namespace test {
    struct RWLockDiag;
};

template <typename T, typename M = hsqr::RWMutex>
class RWLock {
    struct State;

public:
    class ReadGuard;
    class WriteGuard;

    RWLock()
        : m_state(new State())
    {
    }
    ~RWLock() = default;
    RWLock(const RWLock&) = delete;
    RWLock& operator=(const RWLock&) = delete;
    RWLock(RWLock&&) = delete;
    RWLock& operator=(RWLock&&) = delete;

    template <typename... Args>
    RWLock(std::in_place_t p, Args&&... args)
        : m_state(new State(p, std::forward<Args>(args)...))
    {
    }
    ReadGuard read()
    {
        return ReadGuard(m_state);
    }
    WriteGuard write()
    {
        return WriteGuard(m_state);
    }

    class ReadGuard {
    public:
        ReadGuard(std::shared_ptr<State> state)
            : m_state(state)
        {
            m_state->mutex.read_lock();
        }
        ~ReadGuard()
        {
            m_state->mutex.read_unlock();
        }
        ReadGuard(const ReadGuard&) = delete;
        ReadGuard& operator=(const ReadGuard&) const = delete;

        ReadGuard(ReadGuard&&) = default;
        ReadGuard& operator=(ReadGuard&&) = default;

        const T& operator*() const
        {
            return m_state->value;
        }

    private:
        std::shared_ptr<State> m_state;
    };

    class WriteGuard {
    public:
        WriteGuard(std::shared_ptr<State> state)
            : m_state(state)
        {
            m_state->mutex.write_lock();
        }
        ~WriteGuard()
        {
            m_state->mutex.write_unlock();
        }
        WriteGuard(const WriteGuard&) = delete;
        WriteGuard& operator=(const WriteGuard&) const = delete;

        WriteGuard(WriteGuard&&) = default;
        WriteGuard& operator=(WriteGuard&&) = default;

        T& operator*()
        {
            return m_state->value;
        }

        const T& operator*() const
        {
            return m_state->value;
        }

    private:
        std::shared_ptr<State> m_state;
    };

private:
    struct State {
        State()
            : value()
        {
        }
        template <typename... Args>
        State(std::in_place_t, Args&&... args)
            : value(std::forward<Args>(args)...)
        {
        }
        T value;
        M mutex;
    };
    std::shared_ptr<State> m_state;
};

} // namespace

#endif // HSQR_RWLOCK_H_