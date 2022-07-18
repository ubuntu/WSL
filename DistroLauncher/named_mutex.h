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
 *      and both are optional and repeatable, so long as no
 *      exceptions are thrown. In this case, the mutex will be
 *      released and the exception re-thrown.
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

// Templated for testing/dependency injection reasons, always use the alias at the end of the file
template <typename MutexAPI> class NamedMutexWrapper
{
  public:
    NamedMutexWrapper() = delete;
    NamedMutexWrapper(NamedMutexWrapper&) = delete;
    NamedMutexWrapper(NamedMutexWrapper&&) = delete;

    /// Some mutexes are almost never used. This makes for an easy optimization by not initializing them unless needed.
    NamedMutexWrapper(std::wstring_view name, bool lazy_init = false) :
        mutex_handle{nullptr}, mutex_name{mangle_name(name)}
    {
        if (!lazy_init) {
            create();
        }
    }

    virtual ~NamedMutexWrapper() noexcept
    {
        destroy();
    }

    class Lock
    {
      public:
        constexpr Lock() noexcept : parent_(nullptr), response_(0)
        { }

        Lock(Lock& other) = delete;
        Lock(Lock&& other) noexcept : Lock()
        {
            *this = std::move(other);
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
                safe_execute(std::forward<Callable>(f), [&]() noexcept { release(); });
            }
            return std::move(*this);
        }

        template <typename Callable> Lock& or_else(Callable&& f)
        {
            if (!ok()) {
                f();
            }
            return std::move(*this);
        }

      private:
        explicit Lock(NamedMutexWrapper& parent) noexcept : parent_(&parent), response_(parent.wait_and_acquire())
        { }

        NamedMutexWrapper* parent_;
        DWORD response_;

        friend class NamedMutexWrapper;
    };

    Lock lock() noexcept
    {
        return Lock(*this);
    }

    // Adds a prefix to the name to avoid name collisions with other processes
    static std::wstring mangle_name(std::wstring_view lock_name)
    {
        std::wstring prefix{L"WSL_" + DistributionInfo::Name + L"_"};
        return prefix += lock_name;
    }

  protected:
    HANDLE mutex_handle;
    std::wstring mutex_name;

    DWORD create() noexcept
    {
        return MutexAPI::create(mutex_handle, mutex_name.c_str());
    };
    DWORD destroy() noexcept
    {
        return MutexAPI::destroy(mutex_handle, mutex_name.c_str());
    };
    DWORD wait_and_acquire() noexcept
    {
        if (!mutex_handle) {
            DWORD success = create();
            if (success != 0) {
                return success;
            }
        }
        return MutexAPI::wait_and_acquire(mutex_handle, mutex_name.c_str());
    };
    DWORD release() noexcept
    {
        return MutexAPI::release(mutex_handle, mutex_name.c_str());
    };
};

template <typename CallableExec, typename CallablePanic>
void safe_execute(CallableExec&& f, [[maybe_unused]] CallablePanic&& on_error)
{
    if constexpr (noexcept(f())) {
        f();
        return;
    }

    try {
        f();
    } catch (...) {
        on_error();
        throw;
    }
}

struct Win32MutexApi
{
    static constexpr DWORD timeout_ms = 1000;
    static DWORD create(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept;
    static DWORD destroy(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept;
    static DWORD wait_and_acquire(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept;
    static DWORD release(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept;
};

using NamedMutex = NamedMutexWrapper<Win32MutexApi>;
