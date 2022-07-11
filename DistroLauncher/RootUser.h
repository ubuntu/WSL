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
    RootSession(HRESULT* hr);

    // Swaps user back to default
    ~RootSession();
};

HRESULT WslLaunchInteractiveAsRoot(PCWSTR command, BOOL useCurrentWorkingDirectory, DWORD* exitCode);
HRESULT WslLaunchAsRoot(PCWSTR command, BOOL useCurrentWorkingDirectory, HANDLE stdIn, HANDLE stdOut, HANDLE stdErr,
                        HANDLE* process);

template <typename Callable, typename ReturnType = std::result_of_t<typename Callable(void)>>
HRESULT WslAsRoot(Callable f, ReturnType* r)
{
    HRESULT hr{};
    RootSession sudo(&hr);
    if (FAILED(hr)) {
        return hr;
    }

    *r = f();
    return hr;
}

template <typename Callable>
HRESULT WslAsRoot(Callable f)
{
    HRESULT hr{};
    RootSession sudo(&hr);
    if (FAILED(hr)) {
        return hr;
    }

    if constexpr (std::is_same<HRESULT, std::invoke_result_t<Callable(void)>>::value) {
        return f();
    }
    f();
    return hr;
}