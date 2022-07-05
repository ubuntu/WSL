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

    // Does the same as the destructor, but allows for capturing the result
    HRESULT Release();

    // Captures the result of last opening/closing
    HRESULT GetHResult() const noexcept;
};

HRESULT WSLLaunchInteractiveAsRoot(PCWSTR command, BOOL useCurrentWorkingDirectory, DWORD* exitCode);
HRESULT WSLLaunchAsRoot(PCWSTR command, BOOL useCurrentWorkingDirectory, HANDLE stdIn, HANDLE stdOut, HANDLE stdErr,
                        HANDLE* process);