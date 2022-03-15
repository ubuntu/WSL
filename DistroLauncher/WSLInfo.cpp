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

namespace Oobe::internal
{
    namespace
    {
        bool isX11UnixSocketMounted();
    }
    // Although currently only used in this translation unit,
    // this function has a lot of potential to not be exported.
    bool WslLaunchSuccess(const TCHAR* command, DWORD dwMilisseconds)
    {
        using defer = std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&::CloseHandle)>;
        DWORD exitCode;
        HANDLE child = nullptr;

        HRESULT hr = g_wslApi.WslLaunch(command, FALSE, nullptr, nullptr, nullptr, &child);
        if (child == nullptr || FAILED(hr)) {
            return false;
        }
        defer processGuard{child, CloseHandle}; // ensures to call CloseHandle in the end of the scope.

        if (WaitForSingleObject(child, dwMilisseconds) == WAIT_OBJECT_0) {
            auto success = GetExitCodeProcess(child, &exitCode);
            return (success == TRUE && exitCode == 0);
        }

        TerminateProcess(child, WAIT_TIMEOUT);
        return false;
    }

    // Retrieves subsystem version from the WSL API.
    DWORD WslGetDistroSubsystemVersion()
    {
        constexpr unsigned short FOURTHBIT = 0x08;
        ULONG distributionVersion;
        ULONG defaultUID;
        WSL_DISTRIBUTION_FLAGS wslDistributionFlags;
        PSTR* defaultEnvironmentVariables;
        ULONG defaultEnvironmentVariableCount;

        HRESULT hr = g_wslApi.WslGetDistributionConfiguration(&distributionVersion,
                                                              &defaultUID,
                                                              &wslDistributionFlags,
                                                              &defaultEnvironmentVariables,
                                                              &defaultEnvironmentVariableCount);
        if (FAILED(hr) || distributionVersion == 0) {
            Helpers::PrintErrorMessage(hr);
            return 0;
        }

        // discarding the env variable string array otherwise they would leak.
        for (int64_t i = defaultEnvironmentVariableCount - 1; i >= 0; --i) {
            CoTaskMemFree(static_cast<LPVOID>(defaultEnvironmentVariables[i]));
        }

        // Per conversation at https://github.com/microsoft/WSL-DistroLauncher/issues/96
        // the information about version 1 or 2 is on the 4th bit of the distro flags, which is not
        // currently referenced by the API nor docs. The `1+` is just to ensure we return 1 or two.
        return (1 + ((wslDistributionFlags & FOURTHBIT) >> 3));
    }

    inline bool isWslgEnabled()
    {
        return isX11UnixSocketMounted();
    }

    namespace
    {
        // Returns true if the WSL successfully launches a list
        // of commands to ensure X11 socket is properly mounted.
        // One indication that the distro has graphical support enabled.
        bool isX11UnixSocketMounted()
        {
            const std::array cmds = {L"ls -l /tmp/.X11-unix",                // symlink is resolved,
                                     L"ss -lx | grep \"/tmp/.X11-unix/X0\"", // socket is listening,
                                     L"test -n $DISPLAY"};                   // display env var is set.

            // I'm sure is better to read this way than with the algorithm.
            // NOLINTNEXTLINE(readability-use-anyofallof)
            for (const auto* const cmd : cmds) {
                if (!WslLaunchSuccess(cmd)) {
                    wprintf(L"Command %s failed.\n", cmd);
                    return false;
                }
            }

            return true;
        }

    } // namespace.

} // namespace Oobe::internal

// This was kept under Helpers namespace to avoid touching OOBE.cpp/h files.
// TODO: move to Oobe namespace when its time to maintain OOBE.cpp/h
namespace Helpers
{
    bool WslGraphicsSupported()
    {
        // Could WSL 3 or greater exist in the future?
        return (Oobe::internal::isWslgEnabled() && Oobe::internal::WslGetDistroSubsystemVersion() > 1);
    }

} // namespace Helpers.
