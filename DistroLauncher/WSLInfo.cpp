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
        bool isWSLVDCPluginRegistered();
    }

    // Altough currently only used in this translation unit,
    // this function has a lot of potential to not be exported.
    bool WslLaunchSuccess(const TCHAR* command, DWORD dwMilisseconds) {
        DWORD exitCode;
        HANDLE child;
        HRESULT hr = g_wslApi.WslLaunch(command, false, NULL, NULL, NULL, &child);
        if (child == NULL || FAILED(hr)) {
            return false;
        }

        WaitForSingleObject(child, dwMilisseconds);
        auto success = GetExitCodeProcess(child, &exitCode);
        CloseHandle(child);
        if (success == FALSE || exitCode != 0) {
            return false;
        }

        return true;
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
        for (int i = defaultEnvironmentVariableCount-1; i >= 0; --i) {
            CoTaskMemFree((LPVOID)(defaultEnvironmentVariables[i]));
        }

        return distributionVersion;
    }

    inline bool isWslgEnabled() {
        return isX11UnixSocketMounted();
    }

    bool WslGraphicsSupported() {
        // Could WSL 3 or greater exist in the future?
        if(Helpers::isWslgEnabled() &&
            Helpers::WslGetDistroSubsystemVersion() > 1) {
            return true;
        }

        return false;
    }
     
/* =========================== INTERNALS ================================== */
    namespace {
        // Returns true if the WSL successfully launches a list
        // of commands to ensure X11 socket is properly mounted.
        // One indication that the distro has graphical support enabled.
        bool isX11UnixSocketMounted() {
            const std::array cmds = {
                L"ls -l /tmp/.X11-unix",
                L"ss -lx | grep \"/tmp/.X11-unix/X0\"",
            };
            for (auto cmd : cmds) {
                if (!WslLaunchSuccess(cmd, 500)) {
                    wprintf(L"Command %s failed.\n", cmd);
                    return false;
                }
            }
            return true;
        } 
    } // namespace.
} // namespace Helpers.
