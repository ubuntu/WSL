#pragma once
#include "stdafx.h"

/**
* RAII class to switch to root user, and back to default user
*/
class RootSession
{
    HRESULT* hr;
    ULONG defaultUID;
    WSL_DISTRIBUTION_FLAGS wslDistributionFlags;


public:
    RootSession(RootSession&) = delete;
    RootSession(RootSession&&) = delete;
    RootSession operator=(RootSession&) = delete;

    // Swaps user with root
    RootSession(HRESULT *hr);

    // Swaps user back to default
    ~RootSession();

    // Does the same as the destructor, but allows for capturing the result
    HRESULT Release();

    // Captures the result of last opening/closing
    HRESULT GetHResult() const noexcept;
};

HRESULT WSLLaunchInteractiveAsRoot(PCWSTR command, BOOL useCurrentWorkingDirectory, DWORD* exitCode);
HRESULT WSLLaunchAsRoot(PCWSTR command, BOOL useCurrentWorkingDirectory, HANDLE stdIn, HANDLE stdOut, HANDLE stdErr,
    HANDLE* process);