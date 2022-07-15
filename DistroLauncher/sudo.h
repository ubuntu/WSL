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

HRESULT WslGetDefaultUserAndFlags(ULONG& defaultUID, WSL_DISTRIBUTION_FLAGS& wslDistributionFlags);

/**
 * RAII class to switch to root user, and back to default user
 * 
 * This class is very prone to miss-use, so a static API is provided and 
 * all other methods are private.
 */
class Sudo
{
  public:
    // Calls the passed function and returns the worst HRESULT of Root Acquisition and LauchInteractive commands
    static HRESULT WslLaunchInteractive(PCWSTR command, BOOL useCurrentWorkingDirectory, DWORD* exitCode);

    // Calls the passed function and returns the worst HRESULT of Root Acquisition and Lauch commands
    static HRESULT WslLaunch(PCWSTR command, BOOL useCurrentWorkingDirectory, HANDLE stdIn, HANDLE stdOut,
                             HANDLE stdErr, HANDLE* process);

    // Calls the passed function and returns the HRESULT of root acquisition
    template <typename Callable> static HRESULT Run(Callable f)
    {
        try {
            Sudo root_scope_lock;
            if (!root_scope_lock.acquired()) {
                return S_OK;
            }
            f();
            return S_FALSE;
        } catch (std::exception& exception) {
            // Catching-and-rethrowing to destroy the RootSession
            throw exception;
        }
    }

    [[nodiscard]] bool acquired() const noexcept
    {
        return acquired_;
    }

    static constexpr ULONG root_uid = 0;

  private:
    ULONG defaultUID;
    WSL_DISTRIBUTION_FLAGS wslDistributionFlags;
    bool acquired_ = false;

    Sudo(Sudo&) = delete;
    Sudo(Sudo&&) = delete;
    Sudo operator=(Sudo&) = delete;

    Sudo()
    {
        acquire();
    }

    ~Sudo()
    {
        release();
    }

    // Swaps default user with root
    HRESULT acquire();

    // Swaps user back to default
    HRESULT release();
};