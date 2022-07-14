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
#include "dummy_apis.h"

std::wstring mangle_name(std::wstring name)
{
    return L"WSL_" + DistributionInfo::Name + L"_" + name;
}

// Testing Create and Destroy
TEST(NamedMutexTests, CreateAndDestroy)
{
    Testing::MutexMockAPI::reset_back_end();
    auto& dbe = Testing::MutexMockAPI::dummy_back_end;
    std::list<Testing::MutexMockAPI::dummy_mutex>::const_iterator it;
    {
        Testing::NamedMutex mutex(L"test-lifetime");
        it = std::find(dbe.cbegin(), dbe.cend(), mangle_name(L"test-lifetime"));
        ASSERT_NE(it, dbe.cend());  // Added name to database -> Create called
        ASSERT_EQ(it->refcount, 1); // destroy called once -> Create called once
        ASSERT_FALSE(it->locked);   // Not locked -> wait_and_acquire not called

        {
            Testing::NamedMutex mutex_2(L"test-lifetime");
            it = std::find(dbe.cbegin(), dbe.cend(), mangle_name(L"test-lifetime"));
            ASSERT_NE(it, dbe.cend());  // Name still in -> destroy not called
            ASSERT_EQ(it->refcount, 2); // create called twice
            ASSERT_FALSE(it->locked);   // Still not locked -> wait_and_acquire not called
        }

        it = std::find(dbe.cbegin(), dbe.cend(), mangle_name(L"test-lifetime"));
        ASSERT_NE(it, dbe.cend());  // Name not in database -> destroy called once
        ASSERT_EQ(it->refcount, 1); // destroy called once
    }

    it = std::find(dbe.cbegin(), dbe.cend(), mangle_name(L"test-lifetime"));
    ASSERT_EQ(it, dbe.cend()); // Name not in database -> destroy called twice
}

TEST(NamedMutexTests, StateTransitions)
{
    Testing::MutexMockAPI::reset_back_end();
    Testing::NamedMutex lazy_mutex(L"test-state-transitions", true);

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
    Testing::MutexMockAPI::reset_back_end();
    Testing::NamedMutex mutex(L"test-monadic-api");

    // Testing success
    bool and_then = false;
    bool or_else = false;
    Testing::NamedMutex::Lock scope_lock =
      mutex.lock().and_then([&] { and_then = true; }).or_else([&] { or_else = true; });
    ASSERT_TRUE(and_then);
    ASSERT_FALSE(or_else);

    // Testing failure (fails because it's locked already)
    and_then = false;
    or_else = false;
    mutex.lock().and_then([&] { and_then = true; }).or_else([&] { or_else = true; });
    ASSERT_FALSE(and_then);
    ASSERT_TRUE(or_else);
}

TEST(NamedMutexTests, Exceptions)
{
    Testing::MutexMockAPI::reset_back_end();
    Testing::NamedMutex mutex(L"test-exceptions");

    // derived from std::exception
    {
        try {
            mutex.lock().and_then([]() { throw std::runtime_error("Hello!"); });
        } catch (std::runtime_error& err) {
            ASSERT_EQ(err.what(), std::string{"Hello!"});
        } catch (int&) {
            FAIL();
        } catch (...) {
            FAIL();
        }

        std::optional<bool> previous_mutex_released = std::nullopt;

        mutex.lock().and_then([&] { previous_mutex_released = {true}; }).or_else([&]() noexcept {
            previous_mutex_released = {false};
        });

        ASSERT_TRUE(previous_mutex_released.has_value());
        ASSERT_TRUE(previous_mutex_released.value());
    }

    // some other type
    {
        try {
            mutex.lock().and_then([]() { throw 42; });
        } catch (std::runtime_error&) {
            FAIL();
        } catch (int& err) {
            ASSERT_EQ(err, 42);
        } catch (...) {
            FAIL();
        }

        std::optional<bool> previous_mutex_released = std::nullopt;

        mutex.lock().and_then([&] { previous_mutex_released = {true}; }).or_else([&]() noexcept {
            previous_mutex_released = {false};
        });

        ASSERT_TRUE(previous_mutex_released.has_value());
        ASSERT_TRUE(previous_mutex_released.value());
    }
}