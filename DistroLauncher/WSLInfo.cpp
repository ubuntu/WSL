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

namespace Helpers {
    namespace {
        bool isX11UnixSocketMounted();
    }

    // Altough currently only used in this translation unit,
    // this function has a lot of potential to not be exported.
    bool WslLaunchSuccess(const TCHAR* command, DWORD dwMilisseconds) {
        DWORD exitCode;
        HANDLE child;
        HRESULT hr = g_wslApi.WslLaunch(command, FALSE, NULL, NULL, NULL, &child);
        if (child == NULL || FAILED(hr)) {
            return false;
        }

        WaitForSingleObject(child, dwMilisseconds);
        auto success = GetExitCodeProcess(child, &exitCode);
        CloseHandle(child);
        return (success == TRUE && exitCode == 0);
    }

    // Retrieves subsystem version from the WSL API.
    DWORD WslGetDistroSubsystemVersion() {
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

        // discards the env variable string array otherwise they will leak.
        for (int64_t i = defaultEnvironmentVariableCount-1; i >= 0; --i) {
            CoTaskMemFree(static_cast<LPVOID>(defaultEnvironmentVariables[i]));
        }
        
        // Per conversation at https://github.com/microsoft/WSL-DistroLauncher/issues/96
        // distributionVersion is not what I thought.
        // The actual information is on the 4th bit of the distro flags, which is not 
        // currently referrenced by the API nor docs. The `1+` is just to ensure we return 1 or two.
        return (1+((wslDistributionFlags & 0x08)>>3));
    }

    inline bool isWslgEnabled() {
        return isX11UnixSocketMounted();
    }

    bool WslGraphicsSupported() {
        // Could WSL 3 or greater exist in the future?
        return (Helpers::isWslgEnabled() &&
                Helpers::WslGetDistroSubsystemVersion() > 1);
    }
     
/* =========================== INTERNALS ================================== */
    namespace {
        // Returns true if the WSL successfully launches a list
        // of commands to ensure X11 socket is properly mounted.
        // One indication that the distro has graphical support enabled.
        bool isX11UnixSocketMounted() {
            const std::array cmds = {
                L"ls -l /tmp/.X11-unix | grep wslg", // symlink points to wslg.
                L"ls -l /tmp/.X11-unix/", // symlink resolves correctly.
                L"ss -lx | grep \"/tmp/.X11-unix/X0\"", // socket is listening.
            };

            // I'm sure is better to read this way than with the algorithm.
            // NOLINTNEXTLINE(readability-use-anyofallof)
            for (const auto *const cmd : cmds) {
                if (!WslLaunchSuccess(cmd)) {
                    wprintf(L"Command %s failed.\n", cmd);
                    return false;
                }
            }

            return true;
        } 
    } // namespace.
} // namespace Helpers.
