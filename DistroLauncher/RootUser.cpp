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

HRESULT WslGetDefaultUserAndFlags(ULONG& defaultUID, WSL_DISTRIBUTION_FLAGS& wslDistributionFlags)
{
    ULONG distributionVersion;
    PSTR* defaultEnvironmentVariables = nullptr;
    ULONG defaultEnvironmentVariableCount = 0;

    const HRESULT hr =
      g_wslApi.WslGetDistributionConfiguration(&distributionVersion, &defaultUID, &wslDistributionFlags,
                                               &defaultEnvironmentVariables, &defaultEnvironmentVariableCount);

    if (FAILED(hr) || distributionVersion == 0) {
        Helpers::PrintErrorMessage(hr);
        return hr;
    }

    // discarding the env variable string array otherwise they would leak.
    for (ULONG i = 0; i < defaultEnvironmentVariableCount; ++i) {
        CoTaskMemFree(static_cast<LPVOID>(defaultEnvironmentVariables[i]));
    }

    return hr;
}

HRESULT Sudo::acquire()
{
    HRESULT hr = WslGetDefaultUserAndFlags(defaultUID, wslDistributionFlags);

    if (SUCCEEDED(hr)) {
        hr = g_wslApi.WslConfigureDistribution(root_uid, wslDistributionFlags);
        acquired_ = SUCCEEDED(hr);
    }
    return hr;
}

HRESULT Sudo::release()
{
    if (acquired()) {
        return g_wslApi.WslConfigureDistribution(defaultUID, wslDistributionFlags);
        acquired_ = false;
    }
    return S_FALSE;
}

HRESULT Sudo::WslLaunchInteractive(PCWSTR command, BOOL useCurrentWorkingDirectory, DWORD* exitCode)
{
    HRESULT launch_result = S_FALSE;
    auto f = [&]() { launch_result = g_wslApi.WslLaunchInteractive(command, useCurrentWorkingDirectory, exitCode); };
    HRESULT sudo_result = Run(f);

    return FAILED(sudo_result) ? sudo_result : launch_result;
}

HRESULT Sudo::WslLaunch(PCWSTR command, BOOL useCurrentWorkingDirectory, HANDLE stdIn, HANDLE stdOut, HANDLE stdErr,
                        HANDLE* process)
{
    HRESULT launch_result = S_FALSE;
    auto f = [&]() {
        launch_result = g_wslApi.WslLaunch(command, useCurrentWorkingDirectory, stdIn, stdOut, stdErr, process);
    };
    HRESULT sudo_result = Run(f);

    return FAILED(sudo_result) ? sudo_result : launch_result;
}
