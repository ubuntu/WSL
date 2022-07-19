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
 * This file contains a few of dummy API used for testing
 */

namespace Testing
{
    struct MockMutexAPI
    {
        // Instead of a Windows back-end, we have a registry of mock mutexes as our back-end
        struct MockMutex
        {
            std::wstring name;
            bool locked = false;
            unsigned refcount = 1;

            bool operator==(std::wstring_view name) const noexcept;
            friend bool operator==(std::wstring_view name, const MockMutex& mutex) noexcept;
        };

        static std::list<MockMutex> dummy_back_end; // Using list for pointer (HANDLE) stability

        // For testing purposes
        static bool& locked(HANDLE& mutex_handle) noexcept;
        static void reset_back_end();

        // Overriding back-end API
        static DWORD create(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept;
        static DWORD destroy(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept;
        static DWORD wait_and_acquire(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept;
        static DWORD release(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept;
    };

    class MockNamedMutex : public NamedMutexWrapper<MockMutexAPI>
    {
      public:
        MockNamedMutex(std::wstring name, bool lazy_init = false);

        // Exposing internal state for testing
        HANDLE& get_mutex_handle();
        bool& locked();
    };

    struct WslMockAPI
    {
        // Instead of changing users, a pair of variables are changed
        static HRESULT GetDefaultUserAndFlags(ULONG& defaultUID, WSL_DISTRIBUTION_FLAGS& wslDistributionFlags) noexcept;
        static HRESULT SetDefaultUserAndFlags(ULONG defaultUID, WSL_DISTRIBUTION_FLAGS wslDistributionFlags) noexcept;

        static ULONG defaultUID_;
        static WSL_DISTRIBUTION_FLAGS wslDistributionFlags_;

        // Instead of executing commands, they are logged
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

        static HRESULT LaunchInteractive(PCWSTR command, BOOL useCurrentWorkingDirectory, DWORD* exitCode) noexcept;

        static HRESULT Launch(PCWSTR command, BOOL useCurrentWorkingDirectory, HANDLE stdIn, HANDLE stdOut,
                              HANDLE stdErr, HANDLE* process) noexcept;

        static void reset_mock_distro();

        // Log of commands
        static std::vector<Command> command_log;
        static std::vector<InteractiveCommand> interactive_command_log;

        // Mock process, and a mock variable for the HANDLE to point to
        static HANDLE mock_process;
        static int mock_process_impl;
    };

    // Aliases within the Testing namespace
    using NamedMutex = MockNamedMutex;
    using Sudo = SudoInternals::SudoInterface<NamedMutex, Testing::WslMockAPI>;
}
