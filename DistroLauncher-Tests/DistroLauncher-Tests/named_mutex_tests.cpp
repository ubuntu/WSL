/*
 * Copyright (C) 2022 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include "gtest/gtest.h"

struct TestMutexApi
{
    static constexpr DWORD timeout_ms = 1000;
    static DWORD create(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept;
    static DWORD destroy(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept;
    static DWORD wait_and_acquire(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept;
    static DWORD release(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept;

    // Instead of a Windows back-end, we have a registry of dummy mutexes as our back-end
    struct dummy_mutex
    {
        std::wstring name;
        unsigned refcount = 1;
        bool locked = false;

        bool operator==(std::wstring_view name) const
        {
            return name == this->name;
        }
    };

    static std::list<dummy_mutex> dummy_back_end; // Using list for pointer stability

    // For testing purposes
    static bool& locked(HANDLE& mutex_handle) noexcept
    {
        return static_cast<dummy_mutex*>(mutex_handle)->locked;
    }
};

class TestNamedMutex : public NamedMutexWrapper<TestMutexApi>
{
  public:
    TestNamedMutex(std::wstring name, bool lazy_init = false) : NamedMutexWrapper(name, lazy_init)
    { }

    // Exposing internal state for testing
    HANDLE& get_mutex_handle()
    {
        return mutex_handle;
    }
    bool& locked()
    {
        return TestMutexApi::locked(mutex_handle);
    }
};

std::list<TestMutexApi::dummy_mutex> TestMutexApi::dummy_back_end{};

// Overriding back-end API
DWORD TestMutexApi::create(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept
{
    auto it = std::find(dummy_back_end.begin(), dummy_back_end.end(), mutex_name);
    if (it == dummy_back_end.cend()) {
        dummy_back_end.push_back(dummy_mutex{std::wstring(mutex_name)});
        mutex_handle = static_cast<HANDLE>(&dummy_back_end.back());
    } else {
        ++it->refcount;
        mutex_handle = static_cast<HANDLE>(&(*it));
    }
    return 0;
}

DWORD TestMutexApi::destroy(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept
{
    auto it = std::find(dummy_back_end.begin(), dummy_back_end.end(), mutex_name);

    if (it == dummy_back_end.cend()) {
        return 1; // Destroyed a non-existing mutex
    }

    --it->refcount;
    if (it->refcount == 0) {
        dummy_back_end.erase(it);
    }
    return 0;
}

DWORD TestMutexApi::wait_and_acquire(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept
{
    if (locked(mutex_handle))
        return 1;
    locked(mutex_handle) = true;
    return 0;
}

DWORD TestMutexApi::release(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept
{
    locked(mutex_handle) = false;
    return 0;
}

std::wstring mangle_name(std::wstring name)
{
    return L"WSL_" + DistributionInfo::Name + L"_" + name;
}

// Testing Create and Destroy
TEST(NamedMutexTests, CreateAndDestroy)
{
    auto& dbe = TestMutexApi::dummy_back_end;
    std::list<TestMutexApi::dummy_mutex>::const_iterator it;
    {
        TestNamedMutex mutex(L"test_1");
        it = std::find(dbe.cbegin(), dbe.cend(), mangle_name(L"test_1"));
        ASSERT_NE(it, dbe.cend());  // Added name to database -> Create called
        ASSERT_EQ(it->refcount, 1); // destroy called once -> Create called once
        ASSERT_FALSE(it->locked);   // Not locked -> wait_and_acquire not called

        {
            TestNamedMutex mutex_2(L"test_1");
            it = std::find(dbe.cbegin(), dbe.cend(), mangle_name(L"test_1"));
            ASSERT_NE(it, dbe.cend());  // Name still in -> destroy not called
            ASSERT_EQ(it->refcount, 2); // create called twice
            ASSERT_FALSE(it->locked);   // Still not locked -> wait_and_acquire not called
        }

        it = std::find(dbe.cbegin(), dbe.cend(), mangle_name(L"test_1"));
        ASSERT_NE(it, dbe.cend());  // Name not in database -> destroy called once
        ASSERT_EQ(it->refcount, 1); // destroy called once
    }

    it = std::find(dbe.cbegin(), dbe.cend(), mangle_name(L"test_1"));
    ASSERT_EQ(it, dbe.cend()); // Name not in database -> destroy called twice
}

TEST(NamedMutexTests, StateTransitions)
{
    TestNamedMutex lazy_mutex(L"test", true);

    ASSERT_EQ(lazy_mutex.get_mutex_handle(), nullptr); // Initialization delayed

    // Testing scope lock
    {
        auto scope_lock = lazy_mutex.lock();

        // Testing internal state
        ASSERT_NE(lazy_mutex.get_mutex_handle(), nullptr); // Initialization completed
        ASSERT_TRUE(scope_lock.ok());                      // Lock acquired -> called wait_and_acquire
        ASSERT_TRUE(lazy_mutex.locked());                  // Lock acquired -> called wait_and_acquire

        // Ensuring a second lock fails
        auto second_lock = lazy_mutex.lock();
        ASSERT_FALSE(second_lock.ok()); // Lock not acquired

        // Ensuring failure does not invalidate original
        ASSERT_NE(lazy_mutex.get_mutex_handle(), nullptr); // Mutex not destroyed -> destroy not called
        ASSERT_TRUE(scope_lock.ok());                      // Lock not dropped yet -> release not called
        ASSERT_TRUE(lazy_mutex.locked());                  // Lock not dropped yet -> release not called
    }

    ASSERT_NE(lazy_mutex.get_mutex_handle(), nullptr); // Mutex not destroyed -> destroy not called
    ASSERT_FALSE(lazy_mutex.locked());                 // No lock -> release called

    // Testing second (non-simultaneous) lock
    auto scope_lock = lazy_mutex.lock();
    ASSERT_NE(lazy_mutex.get_mutex_handle(), nullptr); // Mutex not destroyed -> destroy not called
    ASSERT_TRUE(scope_lock.ok());                      // Lock acquired -> called wait_and_acquire, not released
    ASSERT_TRUE(lazy_mutex.locked());                  // Lock acquired -> called wait_and_acquire, not released
}

TEST(NamedMutexTests, MonadicInterface)
{
    TestNamedMutex mutex(L"test");

    // Testing success
    bool and_then = false;
    bool or_else = false;
    auto scope_lock = mutex.lock().and_then([&] { and_then = true; }).or_else([&] { or_else = true; });
    ASSERT_TRUE(and_then);
    ASSERT_FALSE(or_else);

    // Testing failure (fails because it's locked alredy)
    and_then = false;
    or_else = false;
    mutex.lock().and_then([&] { and_then = true; }).or_else([&] { or_else = true; });
    ASSERT_FALSE(and_then);
    ASSERT_TRUE(or_else);
}