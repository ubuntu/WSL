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

#pragma once

namespace Oobe::internal
{
    // Returns true if user system has WSLg enabled.
    inline bool isWslgEnabled();

    // Returns true if WSLg is enabled and distro subsystem version is > 1.
    bool WslGraphicsSupported();

    // Returns the subsystem version for this specific distro or 0 if failed.
    DWORD WslGetDistroSubsystemVersion();
    // Runs a Linux command and checks for successful exit.
    // Returns true if all the commands `exit 0`.
    // Launched command will not be interactive nor show any output,
    // but it blocks current thread for up to dwMilisseconds.
    bool WslLaunchSuccess(const TCHAR* command, DWORD dwMilisseconds = 500); // NOLINT(readability-magic-numbers):
    // Magic numbers in default arguments should not be an issue.

    /// Parses an [ini]/conf input stream from the beginning checking for the existence of the entry [section.key].
    /// Returns true if that sections is present and its value contains the string [valueContains].
    /// This is intended to be called once, thus no caching.
    bool ini_find_value(std::wistream& ini, std::wstring_view section, std::wstring_view key,
                        std::wstring_view valueContains);

    /// Returns true if ports bound to wildcard or localhost in the WSL 2 VM should be connectable from the host via
    /// localhost:port.
    /// See
    /// <https://docs.microsoft.com/en-us/windows/wsl/wsl-config#:~:text=WSL%202%20VM.-,localhostForwarding,-boolean>
    bool isLocalhostForwardingEnabled(const std::filesystem::path& wslConfig);

    /// Returns true if the ubuntu-desktop-installer snap is found inside the rootfs.
    /// It's possible that we have only Subiquity or none instead.
    /// That's meant to be called during setup, where filesystem errors are less likely to happen.
    bool hasUdiSnap();

    /// Returns true if the subiquity snap is found inside the rootfs.
    /// It's possible that we have the ubuntu-desktop-installer or none instead.
    /// That's meant to be called during setup, where filesystem errors are less likely to happen.
    bool hasSubiquitySnap();
}

namespace Oobe
{
    std::wstring WrapCommand(std::wstring_view intendedCommand);

    // Returns the preferred file path prefix to access files inside the Linux file system.
    const wchar_t* WslPathPrefix();

    HRESULT WslGetDefaultUserAndFlags(ULONG& defaultUID, WSL_DISTRIBUTION_FLAGS& wslDistributionFlags) noexcept;

    // Returns the windows path to access files inside the Linux file system.
    std::filesystem::path WindowsPath(std::wstring_view distro_path);
    std::filesystem::path WindowsPath(const std::filesystem::path& distro_path);

    bool WslFileExists(const std::filesystem::path& distro_path);
}
