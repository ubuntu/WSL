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

// This namespace holds a set of functions and utility classes to deal
// with Win32 specificities in a modern way.
// It aims to be lighweigth enough to justify not using more robust
// solutions like those found in Boost libraries.
namespace Win32Utils
{
    nonstd::expected<std::string, std::wstring> wide_string_to_utf8(const std::wstring& wide_string);
    nonstd::expected<std::wstring, std::wstring> utf8_to_wide_string(const std::string& utf8str);

    /// Positions [window] at the center of the monitor it's displayed in with the Z-order right after [topWindow].
    /// Returns 0 on success.
    DWORD center_window(HWND window);

    /// Resizes [window] to be equal in size and placement to [topWindow] while preserving [topWindow] on top.
    /// Returns 0 on success.
    DWORD resize_to(HWND window, HWND topWindow);

    // Enumeration class that give named constants referring to the operating system version according to the first
    // build number.
    enum class WinVersion
    {
        Win10 = 10240,
        Win11 = 22000,
    };

    /// Returns the operating system version. Assumes Windows 10 or higher.
    WinVersion os_version();

    /// Returns the operating system build number or 0 on failure.
    DWORD os_build_number();

}
