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
#pragma once

#include "stdafx.h"

/*
 *  Abstract
 *  --------
 *  A RAII class to deal with Windows inter-process synchronization.
 *
 *  Usage
 *  --------
 *  Best is to create a single NamedMutex as a static variable. If you believe
 *  it's not going to be used in most executions, then use
 *  ```
 *  static my_mutex = NamedMutex("unique_identifyier", true);
 *  ```
 *  to enable lazy initialization. Otherwise, use
 *  ```
 *  static my_mutex = NamedMutex("unique_identifyier");
 *  ```
 *
 *  In order to acquire a lock, you have two options.
 *  Use the first one unless you've got a good reason not to.
 *   1. Use the monadic interface:
 *      ```
 *      my_mutex.lock().and_then([](){
 *          // do stuff
 *      }).or_else([](){
 *          throw "Oh, no!";
 *      });
 *      ```
 *      Note that the ordering of the functions doesn't matter,
 *      and both are optional and repeatable.
 *
 *   2. Use a scope guard:
 *      ```
 *      NamedMutex::Guard scope_guard = my_mutex.lock();
 *      if(scope_guard.ok()) {
 *          // do stuff
 *      } else {
 *          throw "Oh, no!";
 *      }
 *      ```
 *      Be careful! If your program throws and it's not caught,
 *      your mutex may not be released. Option 1 takes care of
 *      this automatically, hence why it's recommended.
 */
class NamedMutex
{
  public:
    NamedMutex() = delete;
    NamedMutex(NamedMutex&) = delete;
    NamedMutex(NamedMutex&&) = delete;

    /// Some mutexes are almost never used. This makes for an easy optimization by not initializing them unless needed.
    NamedMutex(std::wstring name, bool lazy_init = false) :
        mutex_handle(nullptr), mutex_name(L"WSL_" + DistributionInfo::Name + L"_" + name)
    {
        if (!lazy_init) {
            create();
        }
    }

    ~NamedMutex()
    {
        destroy();
    }

    class Lock
    {
      public:
        constexpr Lock() noexcept : parent_(nullptr), response_(0)
        { }

        Lock(NamedMutex& parent) noexcept : parent_(&parent), response_(parent.wait_and_acquire())
        { }

        Lock(Lock& other) = delete;
        Lock(Lock&& other) noexcept : Lock()
        {
            *this = Lock(std::move(other));
        }

        ~Lock() noexcept
        {
            release();
        }

        void release() noexcept
        {
            if (ok()) {
                parent_->release();
            }
            parent_ = nullptr;
        }

        Lock& operator=(Lock&& other) noexcept
        {
            if (this != &other) {
                parent_ = std::exchange(other.parent_, nullptr);
                response_ = std::exchange(other.response_, 0);
            }
            return *this;
        }

        bool ok() const noexcept
        {
            return parent_ && (response_ == WAIT_OBJECT_0);
        }

        DWORD why() const noexcept
        {
            return parent_ ? 0 : response_;
        }

        template <typename Callable> Lock& and_then(Callable&& f)
        {
            if (ok()) {
                try {
                    f();
                } catch (std::exception& exception) {
                    release();
                    throw exception;
                } catch (...) {
                    release();
                    throw;
                }
            }
            return *this;
        }

        template <typename Callable> Lock& or_else(Callable&& f)
        {
            if (!ok()) {
                f();
            }
            return *this;
        }

      private:
        NamedMutex* parent_;
        DWORD response_;

        friend class NamedMutex;
    };

    Lock lock()
    {
        return Lock(*this);
    }

  private:
    HANDLE mutex_handle;
    std::wstring mutex_name;

    DWORD create() noexcept;
    DWORD destroy() noexcept;
    DWORD wait_and_acquire() noexcept;
    DWORD release() noexcept;
};
