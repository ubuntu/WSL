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

// Fake policy classes to exercise the logic behind the installer controller.
namespace Oobe
{
    static const wchar_t* cmd = L"sudo /usr/libexec/wsl-setup";
    struct NothingWorksPolicy
    {
        static const wchar_t* OobeCommand;

        static bool is_oobe_available()
        {
            return false;
        }

        static std::wstring prepare_prefill_info()
        {
            return L"";
        }

        static bool must_run_in_text_mode()
        {
            return false;
        }

        static void handle_exit_status()
        { }

        static bool copy_file_into_distro(const std::filesystem::path& from, const std::wstring& to)
        {
            return false;
        }

        static bool poll_success(const wchar_t* command, int repeatTimes, HANDLE monitoredProcess)
        {
            return false;
        }

        static DWORD consume_process(HANDLE process, DWORD timeout)
        {
            return -1;
        }

        static HANDLE start_installer_async(const wchar_t* command,
                                            const wchar_t* watcher = L"ss -lx | grep subiquity &>/dev/null")
        {
            return nullptr;
        }

        static DWORD do_launch_sync(const wchar_t* cli)
        {
            return -1;
        }

        static HWND try_hiding_installer_window(int repeatTimes)
        {
            return nullptr;
        }

        static void show_window(HWND window)
        { }
    };

    const wchar_t* NothingWorksPolicy::OobeCommand = cmd; // unused

    struct EverythingWorksPolicy
    {
        static const wchar_t* OobeCommand;

        static bool is_oobe_available()
        {
            return true;
        }

        static std::wstring prepare_prefill_info()
        {
            return L"";
        }

        static bool must_run_in_text_mode()
        {
            return false;
        }

        static void handle_exit_status()
        { }

        static bool copy_file_into_distro(const std::filesystem::path& from, const std::wstring& to)
        {
            return true;
        }

        static bool poll_success(const wchar_t* command, int repeatTimes, HANDLE monitoredProcess)
        {
            return true;
        }

        static DWORD consume_process(HANDLE process, DWORD timeout)
        {
            return 0;
        }

        static HANDLE start_installer_async(const wchar_t* command,
                                            const wchar_t* watcher = L"ss -lx | grep subiquity &>/dev/null")
        {
            return GetStdHandle(STD_OUTPUT_HANDLE);
        }

        static DWORD do_launch_sync(const wchar_t* cli)
        {
            return 0;
        }

        static HWND try_hiding_installer_window(int repeatTimes)
        {
            return static_cast<HWND>(GetStdHandle(STD_OUTPUT_HANDLE));
        }

        static void show_window(HWND window)
        { }
    };

    const wchar_t* EverythingWorksPolicy::OobeCommand = cmd;

    struct FailsToLaunchPolicy
    {
        static const wchar_t* OobeCommand;

        static bool is_oobe_available()
        {
            return true;
        }

        static std::wstring prepare_prefill_info()
        {
            return L"";
        }

        static bool must_run_in_text_mode()
        {
            return false;
        }

        static void handle_exit_status()
        { }

        static bool copy_file_into_distro(const std::filesystem::path& from, const std::wstring& to)
        {
            return true;
        }
        // Socket is never listening.
        static bool poll_success(const wchar_t* command, int repeatTimes, HANDLE monitoredProcess)
        {
            return true;
        }

        static DWORD consume_process(HANDLE process, DWORD timeout)
        {
            return 0;
        }

        static HANDLE start_installer_async(const wchar_t* command,
                                            const wchar_t* watcher = L"ss -lx | grep subiquity &>/dev/null")
        {
            return nullptr; // no child process.
        }

        static HWND try_hiding_installer_window(int repeatTimes)
        {
            return nullptr;
        }

        static void show_window(HWND window)
        { }

        static DWORD do_launch_sync(const wchar_t* cli)
        {
            return -1;
        }
    };
    const wchar_t* FailsToLaunchPolicy::OobeCommand = cmd;

    struct OobeCrashDetectedPolicy
    {
        static const wchar_t* OobeCommand;

        static bool is_oobe_available()
        {
            return true;
        }

        static std::wstring prepare_prefill_info()
        {
            return L"";
        }

        static bool must_run_in_text_mode()
        {
            return false;
        }

        static void handle_exit_status()
        { }

        static bool copy_file_into_distro(const std::filesystem::path& from, const std::wstring& to)
        {
            return true;
        }

        static bool poll_success(const wchar_t* command, int repeatTimes, HANDLE monitoredProcess)
        {
            return true;
        }

        static DWORD consume_process(HANDLE process, DWORD timeout)
        {
            return -1;
        }

        static HANDLE start_installer_async(const wchar_t* command,
                                            const wchar_t* watcher = L"ss -lx | grep subiquity &>/dev/null")
        {
            return GetStdHandle(STD_OUTPUT_HANDLE);
        }

        static DWORD do_launch_sync(const wchar_t* cli)
        {
            return -1;
        }

        static HWND try_hiding_installer_window(int repeatTimes)
        {
            return static_cast<HWND>(GetStdHandle(STD_OUTPUT_HANDLE));
        }

        static void show_window(HWND window)
        { }
    };
    const wchar_t* OobeCrashDetectedPolicy::OobeCommand = cmd;
}