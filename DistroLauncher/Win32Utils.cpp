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

                if (GetWindowRect(window, &rect)==FALSE) {
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
                if (SetWindowPos(window, 0, x, y, width, height, flags)==FALSE) {
                    return GetLastError();
                }

                return 0;
            }

            /// Applies this placement to [window] according to [flags], while preserving [topWindow] on top. Returns 0
            /// on success. See docs for SetWindowPos function.
            DWORD place(HWND window, HWND topWindow, DWORD flags) const
            {
                if (SetWindowPos(window, topWindow, x, y, width, height, flags)==FALSE) {
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

} // namespace Win32Utils
