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
 * This file contains a few of dummy API used for testing
 */

namespace Testing
{
    struct MutexMockAPI
    {
        // Instead of a Windows back-end, we have a registry of dummy mutexes as our back-end
        struct dummy_mutex
        {
            std::wstring name;
            unsigned refcount = 1;
            bool locked = false;

            bool operator==(std::wstring_view name) const noexcept
            {
                return name == this->name;
            }

            friend bool operator==(std::wstring_view name, dummy_mutex mutex) noexcept
            {
                return mutex == name;
            }
        };

        static std::list<dummy_mutex> dummy_back_end; // Using list for pointer stability

        // For testing purposes
        static bool& locked(HANDLE& mutex_handle) noexcept
        {
            return static_cast<dummy_mutex*>(mutex_handle)->locked;
        }

        static void reset_back_end()
        {
            // Removes all entries added (except for a selected set which is kept)
            std::array keep = {L"WSL_UbuntuDev.WslID.Dev_sudo-test-mutex"};
            dummy_back_end.remove_if(
              [&](auto mutex) { return std::find(keep.cbegin(), keep.cend(), mutex) == keep.cend(); });
        }

        // Overriding back-end API
        static DWORD MutexMockAPI::create(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept
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

        static DWORD MutexMockAPI::destroy(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept
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

        static DWORD MutexMockAPI::wait_and_acquire(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept
        {
            if (locked(mutex_handle))
                return 1;
            locked(mutex_handle) = true;
            return 0;
        }

        static DWORD MutexMockAPI::release(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept
        {
            locked(mutex_handle) = false;
            return 0;
        }
    };

    inline std::list<MutexMockAPI::dummy_mutex> MutexMockAPI::dummy_back_end{};

    class NamedMutex : public NamedMutexWrapper<MutexMockAPI>
    {
      public:
        NamedMutex(std::wstring name, bool lazy_init = false) : NamedMutexWrapper(name, lazy_init)
        {
        }

        // Exposing internal state for testing
        HANDLE& get_mutex_handle()
        {
            return mutex_handle;
        }
        bool& locked()
        {
            return MutexMockAPI::locked(mutex_handle);
        }
    };

    struct WslMockAPI
    {
        static HRESULT GetDefaultUserAndFlags(ULONG& defaultUID, WSL_DISTRIBUTION_FLAGS& wslDistributionFlags)
        {
            defaultUID = defaultUID_;
            wslDistributionFlags = wslDistributionFlags_;
            return S_OK;
        }

        static HRESULT SetDefaultUserAndFlags(ULONG defaultUID, WSL_DISTRIBUTION_FLAGS wslDistributionFlags) noexcept
        {
            defaultUID_ = defaultUID;
            wslDistributionFlags_ = wslDistributionFlags;
            return S_OK;
        }

        struct Command
        {
            PCWSTR command;
            BOOL useCurrentWorkingDirectory;
            HANDLE stdIn;
            HANDLE stdOut;
            HANDLE stdErr;
        };

        struct InteractiveCommand
        {
            PCWSTR command;
            BOOL useCurrentWorkingDirectory;
        };

        static HRESULT LaunchInteractive(PCWSTR command, BOOL useCurrentWorkingDirectory, DWORD* exitCode) noexcept
        {
            try {
                interactive_command_log.emplace_back<InteractiveCommand>({command, useCurrentWorkingDirectory});
                *exitCode = 0;
                return S_OK;
            } catch (...) {
                *exitCode = 1;
                return -1; // Failed: hr<0
            }
        }

        static HRESULT Launch(PCWSTR command, BOOL useCurrentWorkingDirectory, HANDLE stdIn, HANDLE stdOut,
                              HANDLE stdErr, HANDLE* process) noexcept
        {
            try {
                *process = mock_process;
                command_log.emplace_back<Command>({command, useCurrentWorkingDirectory, stdIn, stdOut, stdErr});
                return S_OK;
            } catch (...) {
                return -1; // Failed: hr<0
            }
        }

        static void reset_mock_distro()
        {
            command_log = {};
            interactive_command_log = {};
            defaultUID_ = 1;
            wslDistributionFlags_ = WSL_DISTRIBUTION_FLAGS{};
            MutexMockAPI::reset_back_end();
        }

        static HANDLE mock_process;
        static std::vector<Command> command_log;
        static std::vector<InteractiveCommand> interactive_command_log;
        static ULONG defaultUID_;
        static WSL_DISTRIBUTION_FLAGS wslDistributionFlags_;

        static int mock_process_impl;
    };

    using Sudo = SudoInternals::SudoInterface<NamedMutex, Testing::WslMockAPI>;
}

inline Testing::NamedMutex Testing::Sudo::sudo_mutex = Testing::NamedMutex(L"sudo-test-mutex");

inline int Testing::WslMockAPI::mock_process_impl = 0;
inline HANDLE Testing::WslMockAPI::mock_process = static_cast<HANDLE>(&Testing::WslMockAPI::mock_process_impl);

inline std::vector<Testing::WslMockAPI::Command> Testing::WslMockAPI::command_log{};
inline std::vector<Testing::WslMockAPI::InteractiveCommand> Testing::WslMockAPI::interactive_command_log{};
inline ULONG Testing::WslMockAPI::defaultUID_ = 1;
inline WSL_DISTRIBUTION_FLAGS Testing::WslMockAPI::wslDistributionFlags_ = WSL_DISTRIBUTION_FLAGS{};
