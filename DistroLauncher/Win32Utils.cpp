/*
 * Copyright (C) 2021 Canonical Ltd
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
#include <ShlObj_core.h>

namespace Win32Utils
{
    nonstd::expected<std::wstring, std::wstring> utf8_to_wide_string(const std::string& utf8str)
    {
        if (utf8str.empty()) {
            return L"";
        }

        const auto size_needed =
          MultiByteToWideChar(CP_UTF8, 0, &utf8str.at(0), static_cast<int>(utf8str.size()), nullptr, 0);
        if (size_needed <= 0) {
            nonstd::make_unexpected(L"MultiByteToWideChar() failed. Win32 error code: " +
                                    std::to_wstring(GetLastError()));
        }

        std::wstring result(size_needed, 0);
        auto convResult =
          MultiByteToWideChar(CP_UTF8, 0, &utf8str.at(0), static_cast<int>(utf8str.size()), &result.at(0), size_needed);
        if (convResult == 0) {
            nonstd::make_unexpected(L"MultiByteToWideChar() failed. Win32 error code: " +
                                    std::to_wstring(GetLastError()));
        }
        return result;
    }

    nonstd::expected<std::string, std::wstring> wide_string_to_utf8(const std::wstring& wide_str)
    {
        if (wide_str.empty()) {
            return "";
        }

        const auto size_needed = WideCharToMultiByte(CP_UTF8, 0, &wide_str.at(0), static_cast<int>(wide_str.size()),
                                                     nullptr, 0, nullptr, nullptr);
        if (size_needed <= 0) {
            nonstd::make_unexpected(L"WideCharToMultiByte() failed. Win32 error code: " +
                                    std::to_wstring(GetLastError()));
        }

        std::string result(size_needed, 0);
        auto convResult = WideCharToMultiByte(CP_UTF8, 0, &wide_str.at(0), static_cast<int>(wide_str.size()),
                                              &result.at(0), size_needed, nullptr, nullptr);

        if (convResult == 0) {
            nonstd::make_unexpected(L"WideCharToMultiByte() failed. Win32 error code: " +
                                    std::to_wstring(GetLastError()));
        }
        return result;
    }

    namespace
    {

        /// Groups coordinates and sizes to simplify the SetWindowPos API for the cases when we want to center a window
        /// on the display or resize it to the same size of another window. There is no current usage for this class
        /// outside of the following two functions, so let's keep it local until the need arises.
        struct placement
        {
            int x = -1, y = -1, width = -1, height = -1;

            /// Attempts to construct a placement object with sizes and coordinates equal to the [window].
            static nonstd::expected<placement, DWORD> from_window(HWND window)
            {
                assert(window != nullptr);

                RECT rect;

                if (GetWindowRect(window, &rect) == FALSE) {
                    return nonstd::make_unexpected(GetLastError());
                }
                auto width = rect.right - rect.left;
                auto height = rect.bottom - rect.top;
                return placement{rect.left, rect.top, width, height};
            }

            /// Attempts to construct a placement object with [width] and [height] set, but with x and y coordinates
            /// calculated so the window can be placed centered on the screen.
            static nonstd::expected<placement, DWORD> centered_on_monitor(HMONITOR& monitor, int width, int height)
            {
                MONITORINFO mInfo;
                mInfo.cbSize = sizeof(MONITORINFO);
                if (!GetMonitorInfo(monitor, &mInfo)) {
                    return nonstd::make_unexpected(GetLastError());
                }
                int x = (mInfo.rcWork.right - width) / 2;
                int y = (mInfo.rcWork.bottom - height) / 2;
                return placement{x, y, width, height};
            }

            /// Applies this placement to [window] according to [flags]. Returns 0 on success.
            /// See docs for SetWindowPos function.
            DWORD place(HWND window, DWORD flags) const
            {
                // NOLINTNEXTLINE(modernize-use-nullptr)
                if (SetWindowPos(window, 0, x, y, width, height, flags) == FALSE) {
                    return GetLastError();
                }

                return 0;
            }

            /// Applies this placement to [window] according to [flags], while preserving [topWindow] on top. Returns 0
            /// on success. See docs for SetWindowPos function.
            DWORD place(HWND window, HWND topWindow, DWORD flags) const
            {
                if (SetWindowPos(window, topWindow, x, y, width, height, flags) == FALSE) {
                    return GetLastError();
                }

                return 0;
            }
        };

    } // namespace

    DWORD resize_to(HWND window, HWND topWindow)
    {
        assert(window != nullptr);
        assert(topWindow != nullptr);
        auto targetResult = placement::from_window(topWindow);
        if (!targetResult.has_value()) {
            return targetResult.error();
        }

        placement& target = targetResult.value();
        return target.place(window, topWindow, SWP_SHOWWINDOW);
    }

    DWORD center_window(HWND window)
    {
        assert(window != nullptr);
        HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
        auto current = placement::from_window(window);
        if (!current.has_value()) {
            return current.error();
        }

        auto targetResult = placement::centered_on_monitor(monitor, current.value().width, current.value().height);
        if (!targetResult.has_value()) {
            return targetResult.error();
        }

        placement& target = targetResult.value();
        return target.place(window, SWP_SHOWWINDOW);
    }

    DWORD read_build_from_registry()
    {
        constexpr auto REGBUFSIZE = 20;
        DWORD bufSize = REGBUFSIZE;
        wchar_t buffer[REGBUFSIZE] = {0};
        DWORD valueType = 0;
        // NOLINTNEXTLINE(performance-no-int-to-ptr) - Win32 default HKEY's are pointers converted from integer values.
        auto res = RegGetValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                               L"CurrentBuildNumber", RRF_RT_REG_SZ, &valueType, static_cast<void*>(buffer), &bufSize);
        if (res != ERROR_SUCCESS) {
            return 0;
        }
        std::wstring readValue(buffer, bufSize);
        DWORD build = 0;
        try {
            build = std::stoul(readValue);
        } catch (const std::invalid_argument& ex) {
            std::cerr << ex.what() << '\n';
            return 0;
        } catch (const std::out_of_range& ex) {
            std::cerr << ex.what() << '\n';
            return 0;
        }

        return build;
    }

    WinVersion from_build_number(DWORD buildNo)
    {
        // Note for future self: augment this from the top on newer OS version releases.
        if (buildNo >= static_cast<DWORD>(WinVersion::Win11)) {
            return WinVersion::Win11;
        }
        if (buildNo >= static_cast<DWORD>(WinVersion::Win10)) {
            return WinVersion::Win10;
        }
        // lets presume windows 10 on error.
        return WinVersion::Win10;
    }

    WinVersion os_version()
    {
        static const auto version = from_build_number(os_build_number());
        return version;
    }

    DWORD os_build_number()
    {
        static const auto build = read_build_from_registry();
        return build;
    }

    std::filesystem::path homedir()
    {
        wchar_t* pathStr = nullptr;
        auto hRes = SHGetKnownFolderPath(FOLDERID_Profile, KF_FLAG_DEFAULT, nullptr, &pathStr);
        // Ensures the memory allocated by the SHGetKnownFolderPath gets cleaned in the end of the scope no matter what.
        // Simpler than writing a class just for that.
        std::unique_ptr<wchar_t, decltype(&::CoTaskMemFree)> scope_guard{pathStr, ::CoTaskMemFree};
        if (FAILED(hRes)) {
            return std::filesystem::path();
        }
        return std::filesystem::path(pathStr);
    }

    std::filesystem::path thisAppRootdir()
    {
        TCHAR launcherName[MAX_PATH];
        DWORD fnLength = GetModuleFileName(nullptr, launcherName, MAX_PATH);
        if (fnLength == 0) {
            return std::filesystem::path();
        }
        std::filesystem::path exePath{std::wstring_view{launcherName, fnLength}};
        exePath.remove_filename();

        return exePath;
    }
} // namespace Win32Utils
