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
    const ULONG uid = DistributionInfo::QueryUid(L"root");
    *hr = g_wslApi.WslConfigureDistribution(uid, wslDistributionFlags);
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