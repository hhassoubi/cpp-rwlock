#include <cassert>
#include <functional>
#include <hsqr/rwmutex.h>
#include <iostream>
#include <vector>

using namespace hsqr;
using namespace hsqr::test;

struct hsqr::test::RWMutexDiag {
    enum WriteState {
        WriteNone,
        WriteWaiting,
        WriteOwned
    };
    template <typename M>
    static int GetReadCount(M& mu)
    {
        return mu.m_counter.load().reads;
    }
    template <typename M>
    static WriteState GetWriteState(M& mu)
    {
        return WriteState(mu.m_counter.load().write);
    }
    template <typename M>
    static int IsLocked(M& mu)
    {
        if (mu.m_mutex.try_lock()) {
            mu.m_mutex.unlock();
            return false;
        }
        return true;
    }
};

void test_multi_read()
{
    RWMutex m;
    bool unlock_t1 = false;
    std::atomic<int> counter { 0 };

    auto f = [&]() {
        m.read_lock();
        ++counter;
        while (unlock_t1 == false) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        m.read_unlock();
    };

    constexpr int N = 10;
    std::vector<std::thread> v;
    for (int i = 0; i < N; ++i) {
        v.push_back(std::thread { f });
    }

    std::thread test_thread([&]() {
        constexpr int wait_time = 100;
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        assert(counter.load() == N);
        assert(RWMutexDiag::GetReadCount(m) == N);
        assert(RWMutexDiag::GetWriteState(m) == RWMutexDiag::WriteNone);
        assert(RWMutexDiag::IsLocked(m) == false);
        unlock_t1 = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        assert(RWMutexDiag::GetReadCount(m) == 0);
        assert(RWMutexDiag::GetWriteState(m) == RWMutexDiag::WriteNone);
        assert(RWMutexDiag::IsLocked(m) == false);
    });

    for (auto& t : v) {
        t.join();
    }

    test_thread.join();
}

void test_write()
{
    RWMutex m;
    constexpr size_t N = 10;
    std::vector<std::atomic<bool>> unlock(N);
    std::atomic<int> active_thread { -1 };

    auto f = [&](int thread_index) {
        m.write_lock();
        active_thread.store(thread_index);
        while (unlock[thread_index].load() == false) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        active_thread.store(-1);
        m.write_unlock();
    };

    std::vector<std::thread> v;
    for (int i = 0; i < N; ++i) {
        unlock[i].store(false);
        v.push_back(std::thread { std::bind(f, i) });
    }

    std::thread test_thread([&]() {
        constexpr int wait_time = 10;
        for (int i = 0; i < N; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
            int current = active_thread.load();
            assert(current >= 0 && current < N);
            assert(RWMutexDiag::GetReadCount(m) == 0);
            assert(RWMutexDiag::GetWriteState(m) == RWMutexDiag::WriteOwned);
            assert(RWMutexDiag::IsLocked(m) == true);
            unlock[current].store(true);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        assert(RWMutexDiag::GetReadCount(m) == 0);
        assert(RWMutexDiag::GetWriteState(m) == RWMutexDiag::WriteNone);
        assert(RWMutexDiag::IsLocked(m) == false);
    });

    for (auto& t : v) {
        t.join();
    }

    test_thread.join();
}

void test_multi_read_one_write()
{
    RWMutex m;
    constexpr size_t N = 2 * 5; // use even number
    std::vector<std::atomic<bool>> unlock(N + 1); // +1 for the write thread
    std::atomic<int> active_threads { 0 };

    auto f_read = [&](int thread_index) {
        m.read_lock();
        ++active_threads;
        while (unlock[thread_index].load() == false) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        m.read_unlock();
        --active_threads;
    };

    auto f_write = [&]() {
        m.write_lock();
        while (unlock[N].load() == false) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        m.write_unlock();
    };

    std::vector<std::thread> read_threads;
    std::thread write_thread;
    constexpr int wait_time = 10;
    for (int i = 0; i < N; ++i) {
        if (i == N / 2) {
            // start write in the midway
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
            unlock[N].store(false);
            write_thread = std::thread(f_write);
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        }

        unlock[i].store(false);
        read_threads.push_back(std::thread { std::bind(f_read, i) });
    }

    std::thread test_thread([&]() {
        for (int i = 0; i < N / 2; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
            assert(active_threads.load() == N / 2 - i);
            assert(RWMutexDiag::GetReadCount(m) == N / 2 - i);
            assert(RWMutexDiag::GetWriteState(m) == RWMutexDiag::WriteWaiting);
            assert(RWMutexDiag::IsLocked(m) == true);
            // unlock read thread i
            unlock[i].store(true);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        assert(RWMutexDiag::GetReadCount(m) == 0);
        assert(RWMutexDiag::GetWriteState(m) == RWMutexDiag::WriteOwned);
        assert(RWMutexDiag::IsLocked(m) == true);
        // unlock write thread
        unlock[N].store(true);
        for (int i = N / 2; i < N; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
            assert(active_threads.load() == N - i);
            assert(RWMutexDiag::GetReadCount(m) == N - i);
            assert(RWMutexDiag::GetWriteState(m) == RWMutexDiag::WriteNone);
            assert(RWMutexDiag::IsLocked(m) == false);
            // unlock read thread i
            unlock[i].store(true);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        assert(RWMutexDiag::GetReadCount(m) == 0);
        assert(RWMutexDiag::GetWriteState(m) == RWMutexDiag::WriteNone);
        assert(RWMutexDiag::IsLocked(m) == false);
    });

    for (auto& t : read_threads) {
        t.join();
    }

    write_thread.join();
    test_thread.join();
}

void test_dead_lock_detector()
{
    {
        RWMutexChecked m;
        m.read_lock();
        m.read_lock();
    }
    {
        RWMutexChecked m;
        bool good = false;
        m.write_lock();
        try {
            m.write_lock();
        } catch (std::logic_error&) {
            good = true;
        }
        assert(good);
    }
    {
        RWMutexChecked m;
        bool good = false;
        m.read_lock();
        try {
            m.write_lock();
        } catch (std::logic_error&) {
            good = true;
        }
        assert(good);
    }
    {
        RWMutexChecked m;
        bool good = false;
        m.write_lock();
        try {
            m.read_lock();
        } catch (std::logic_error&) {
            good = true;
        }
        assert(good);
    }
}

int main()
{
    test_multi_read();
    test_write();
    test_multi_read_one_write();
    test_dead_lock_detector();
    return 0;
}