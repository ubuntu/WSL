#pragma once
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

/*
 *  Abstract
 *  --------
 *  A interprocess-mutex-protected RAII class to perform commands as root user.
 *
 *  Tips
 *  --------
 *  Performing filesystem operations on WSL from windows requires a restart after
 *  switching the user to root.
 *  ```
 *  Sudo()
 *      .and_then(rebootDistro) // rebootDistro is a function implemented elsewhere
 *      .and_then([&]{ std::filesystem::copy_file(file, L"/etc/myapp/config"); })
 *      .or_else([] { std::wcout << L"Failed to get sudo access!"; });
 *  ```
 *  However, linux commands do not require a reboot.
 *
 *  Usage
 *  --------
 *  There are three interface styles. The first two are valid, and equally safe.
 *  The last one should be avoided.
 *
 *                             MONADIC INTERFACE (UNSAFE)
 * 
 *  This interface uses `and_then` and `or_else` in order to perform actions.
 *  Example:
 *  ```
 *  Sudo()
 *      .and_then([&s]{ s = do_stuff(); })
 *      .or_else([] { std::wcerr << L"Failed to get sudo access!"; });
 *  ```
 *  You may also use a lambda with an input parameter of type Sudo::Status to get the error code:
 *  ```
 *  Sudo()
 *      .and_then([&s]{ s = do_stuff(); })
 *      .or_else([](auto why) { std::wcerr << "Error!\nCode: " << static_cast<int>(why) << std::endl; });
 *  ```
 *  The main advantage is launching multuiple commands without having to lock
 *  and release the mutex, because the and_then can be chained.
 *
 *                           WSL API INTERFACE (SAFE)
 * 
 * This interface is modelled after WSL's windows API:
 * ```
 * DWORD exitCode = 0;
 * HRESULT hr = g_wslApi.WslLaunchInteractive(L"rm -rf /etc/myapp/tmp", TRUE, &exitCode);   // Fails
 * HRESULT hr = Sudo::WslLaunchInteractive(L"rm -rf /etc/myapp/tmp", TRUE, &exitCode);      // Succeeds
 * ```
 *
 *                        SCOPE LOCK INTERFACE (UNSAFE)
 * 
 * Works like a scope lock. Do not use unless you have a good reason to!
 * ```
 * {
 *     auto scope_sudo = Sudo();
 *     if(sudo.ok()) {
 *        data.interesting_function();
 *        // If an exception is thrown and not caught, default user is not reset!
 *     } else {
 *         log_error(sudo.why());
 *     }
 * } // If no exceptions are thrown, sudo is released here.
 * ```
 * In case of doubt, use the follwing expression instead:
 * ```
 * Sudo()
 *     .and_then([&] { data.interesing_function(); })
 *     .transform([] (auto why) { log_error(why); });
 * ```
 */
class Sudo
{
  public:
    enum class Status
    {
        OK = 0,
        FAILED_MUTEX,
        FAILED_GET_USER,
        FAILED_SET_ROOT,
        INACTIVE
    };

    Sudo() noexcept;
    Sudo(Sudo&) = delete;
    Sudo::Sudo(Sudo&& other) noexcept;
    ~Sudo();

    Sudo& operator=(Sudo& other) = delete;

    bool ok() const noexcept
    {
        return status == Status::OK;
    }

    Status why() const noexcept
    {
        return status;
    }

    // Monadic interface
    template <typename Callable> Sudo&& and_then(Callable&& f)
    {
        if (ok()) {
            safe_execute(std::forward<Callable>(f), [&] () noexcept { reset_user(); });
        }
        return std::move(*this);
    }

    template <typename Callable> Sudo&& or_else(Callable&& f)
    {
        // Choosing overload
        constexpr bool f_status = std::is_invocable_v<Callable, Status>;
        constexpr bool f_void = !f_status && std::is_invocable_v<Callable>;

        static_assert(f_void || f_status, "Callable must be ether 'T function()' or 'T function(Sudo::Status)'");

        if (!ok()) {
            if constexpr (f_void) {
                f();
            }
            if constexpr (f_status) {
                f(why());
            }
        }
        return std::move(*this);
    }

    // wsl API interface
    static HRESULT Sudo::WslLaunchInteractive(PCWSTR command, BOOL useCurrentWorkingDirectory, DWORD* exitCode);
    static HRESULT Sudo::WslLaunch(PCWSTR command, BOOL useCurrentWorkingDirectory, HANDLE stdIn, HANDLE stdOut,
                                   HANDLE stdErr, HANDLE* process);

  private:
    static NamedMutex sudo_mutex;

    void reset_user() noexcept;

    NamedMutex::Lock mutex_lock{};

    Status status = Status::INACTIVE;
    ULONG default_user_id = 0;
    WSL_DISTRIBUTION_FLAGS wsl_distribution_flags{};
};
