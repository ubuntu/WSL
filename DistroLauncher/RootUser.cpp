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

RootSession::RootSession(HRESULT* hr_ptr)
{
    hr = hr_ptr;

    ULONG distributionVersion;
    PSTR* defaultEnvironmentVariables = nullptr;
    ULONG defaultEnvironmentVariableCount = 0;

    *hr = g_wslApi.WslGetDistributionConfiguration(&distributionVersion, &defaultUID, &wslDistributionFlags,
                                                   &defaultEnvironmentVariables, &defaultEnvironmentVariableCount);

    if (FAILED(*hr) || distributionVersion == 0) {
        Helpers::PrintErrorMessage(*hr);
        return;
    }

    // discarding the env variable string array otherwise they would leak.
    for (ULONG i = 0; i < defaultEnvironmentVariableCount; ++i) {
        CoTaskMemFree(static_cast<LPVOID>(defaultEnvironmentVariables[i]));
    }

    // Switching to ROOT
    constexpr ULONG root_uid = 0;
    *hr = g_wslApi.WslConfigureDistribution(root_uid, wslDistributionFlags);
}

RootSession::~RootSession()
{
    if (SUCCEEDED(*hr)) {
        *hr = g_wslApi.WslConfigureDistribution(defaultUID, wslDistributionFlags);
    }
}

HRESULT WSLLaunchInteractiveAsRoot(PCWSTR command, BOOL useCurrentWorkingDirectory, DWORD* exitCode)
{
    HRESULT hr{};
    RootSession sudo(&hr);
    if (FAILED(hr)) {
        return hr;
    }
    return g_wslApi.WslLaunchInteractive(command, useCurrentWorkingDirectory, exitCode);
}

HRESULT WSLLaunchAsRoot(PCWSTR command, BOOL useCurrentWorkingDirectory, HANDLE stdIn, HANDLE stdOut, HANDLE stdErr,
                        HANDLE* process)
{
    HRESULT hr{};
    RootSession sudo(&hr);
    if (FAILED(hr)) {
        return hr;
    }
    return g_wslApi.WslLaunch(command, useCurrentWorkingDirectory, stdIn, stdOut, stdErr, process);
}