#include "hsqr/rwlock.h"

#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace hsqr;

void test_multi_read()
{
    RWLock<std::string> lk(std::in_place, "Hello");
    bool unlock_t1 = false;
    constexpr size_t N = 10;
    std::vector<std::string> output(N, "");

    auto f = [&](int thread_index) {
        auto v = lk.read();
        output[thread_index] = *v;
        while (unlock_t1 == false) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    std::vector<std::thread> v;
    for (int i = 0; i < N; ++i) {
        v.push_back(std::thread { std::bind(f, i) });
    }

    std::thread test_thread([&]() {
        constexpr int wait_time = 100;
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        for (int i = 0; i < N; ++i) {
            assert(output[i] == "Hello");
        }
        unlock_t1 = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
    });

    for (auto& t : v) {
        t.join();
    }

    test_thread.join();
}

void test_multi_read_one_write()
{
    RWLock<std::string> lk(std::in_place, "One");
    bool unlock_t1 = false;
    bool unlock_t2 = false;
    constexpr size_t N = 2 * 5;
    std::vector<std::string> output(N, "");

    auto f_read = [&](int thread_index) {
        auto v = lk.read();
        output[thread_index] = *v;
        while (unlock_t1 == false) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    std::vector<std::thread> v;
    for (int i = 0; i < N / 2; ++i) {
        v.push_back(std::thread { std::bind(f_read, i) });
    }

    auto f_write = [&]() {
        auto v = lk.write();
        while (unlock_t2 == false) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        *v = "Two";
    };

    std::thread tr(f_write);

    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    for (int i = N / 2; i < N; ++i) {
        v.push_back(std::thread { std::bind(f_read, i) });
    }

    std::thread test_thread([&]() {
        constexpr int wait_time = 100;
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        for (int i = 0; i < N / 2; ++i) {
            assert(output[i] == "One");
        }
        for (int i = N / 2; i < N; ++i) {
            assert(output[i] == "");
        }
        unlock_t1 = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        for (int i = 0; i < N / 2; ++i) {
            assert(output[i] == "One");
        }
        for (int i = N / 2; i < N; ++i) {
            assert(output[i] == "");
        }
        unlock_t2 = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        for (int i = 0; i < N / 2; ++i) {
            assert(output[i] == "One");
        }
        for (int i = N / 2; i < N; ++i) {
            assert(output[i] == "Two");
        }
    });

    for (auto& t : v) {
        t.join();
    }
    tr.join();
    test_thread.join();
}

void test_multi_write()
{
    RWLock<int> lk(std::in_place, -1);
    constexpr size_t N = 10;
    std::vector<bool> unlock_threads(N, false);
    std::vector<bool> output(N, false);
    int outValue = -1;

    auto f = [&](int thread_index) {
        auto v = lk.write();
        *v = thread_index;
        output[thread_index] = true;
        outValue = *v;
        while (unlock_threads[thread_index] == false) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    std::vector<std::thread> v;
    for (int i = 0; i < N; ++i) {
        v.push_back(std::thread { std::bind(f, i) });
    }

    std::thread test_thread([&]() {
        constexpr int wait_time = 10;
        for (int i = 0; i < N; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
            assert(std::count(output.begin(), output.end(), true) == 1);
            auto active_thread = std::find(output.begin(), output.end(), true) - output.begin();
            assert(active_thread == outValue);
            unlock_threads[active_thread] = true;
            // reset output
            output[active_thread] = false;
        }
    });

    for (auto& t : v) {
        t.join();
    }

    test_thread.join();
}

int main()
{
    test_multi_read();
    test_multi_read_one_write();
    test_multi_write();
    return 0;
}