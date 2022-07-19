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
#include "mock_api.h"

namespace Testing
{
    bool MockMutexAPI::MockMutex::operator==(std::wstring_view name) const noexcept
    {
        return name == this->name;
    }

    bool operator==(std::wstring_view name, const MockMutexAPI::MockMutex& mutex) noexcept
    {
        return mutex == name;
    }

    bool& MockMutexAPI::locked(HANDLE& mutex_handle) noexcept
    {
        return static_cast<MockMutexAPI::MockMutex*>(mutex_handle)->locked;
    }

    void MockMutexAPI::reset_back_end()
    {
        // Removes all entries added (except for a selected set which is kept)
        std::array keep = {Testing::NamedMutex::mangle_name(L"root-user")};
        dummy_back_end.remove_if(
          [&](auto mutex) { return std::find(keep.cbegin(), keep.cend(), mutex) == keep.cend(); });
    }

    // Overriding back-end API
    DWORD MockMutexAPI::create(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept
    {
        auto it = std::find(dummy_back_end.begin(), dummy_back_end.end(), mutex_name);
        if (it == dummy_back_end.cend()) {
            dummy_back_end.push_back(MockMutex{std::wstring(mutex_name)});
            mutex_handle = static_cast<HANDLE>(&dummy_back_end.back());
        } else {
            ++it->refcount;
            mutex_handle = static_cast<HANDLE>(&(*it));
        }
        return 0;
    }

    DWORD MockMutexAPI::destroy(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept
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

    DWORD MockMutexAPI::wait_and_acquire(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept
    {
        if (locked(mutex_handle))
            return 1;
        locked(mutex_handle) = true;
        return 0;
    }

    DWORD MockMutexAPI::release(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept
    {
        locked(mutex_handle) = false;
        return 0;
    }

    MockNamedMutex::MockNamedMutex(std::wstring name, bool lazy_init) : NamedMutexWrapper(name, lazy_init)
    { }

    HANDLE& MockNamedMutex::get_mutex_handle()
    {
        return mutex_handle;
    }

    bool& MockNamedMutex::locked()
    {
        return MockMutexAPI::locked(mutex_handle);
    }

    HRESULT WslMockAPI::GetDefaultUserAndFlags(ULONG& defaultUID, WSL_DISTRIBUTION_FLAGS& wslDistributionFlags) noexcept
    {
        defaultUID = defaultUID_;
        wslDistributionFlags = wslDistributionFlags_;
        return S_OK;
    }

    HRESULT WslMockAPI::SetDefaultUserAndFlags(ULONG defaultUID, WSL_DISTRIBUTION_FLAGS wslDistributionFlags) noexcept
    {
        defaultUID_ = defaultUID;
        wslDistributionFlags_ = wslDistributionFlags;
        return S_OK;
    }

    HRESULT WslMockAPI::LaunchInteractive(PCWSTR command, BOOL useCurrentWorkingDirectory, DWORD* exitCode) noexcept
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

    HRESULT WslMockAPI::Launch(PCWSTR command, BOOL useCurrentWorkingDirectory, HANDLE stdIn, HANDLE stdOut,
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

    void WslMockAPI::reset_mock_distro()
    {
        command_log = {};
        interactive_command_log = {};
        defaultUID_ = 0xabcdef;
        wslDistributionFlags_ = WSL_DISTRIBUTION_FLAGS{};
        MockMutexAPI::reset_back_end();
    }

    // Initializing statics
    inline std::list<MockMutexAPI::MockMutex> MockMutexAPI::dummy_back_end{};

    inline int WslMockAPI::mock_process_impl = 0;
    inline HANDLE WslMockAPI::mock_process = static_cast<HANDLE>(&Testing::WslMockAPI::mock_process_impl);

    inline std::vector<WslMockAPI::Command> Testing::WslMockAPI::command_log{};
    inline std::vector<WslMockAPI::InteractiveCommand> Testing::WslMockAPI::interactive_command_log{};
    inline ULONG WslMockAPI::defaultUID_ = 0xabcdef;
    inline WSL_DISTRIBUTION_FLAGS WslMockAPI::wslDistributionFlags_ = WSL_DISTRIBUTION_FLAGS{};
}