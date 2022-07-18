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

DWORD Win32MutexApi::create(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept
{
    mutex_handle = CreateMutex(nullptr, FALSE, mutex_name);
    if (mutex_handle == nullptr) {
        const auto error = GetLastError();
        return error != 0 ? error
                          : ~static_cast<DWORD>(0); // Ensuring return value is always != 0 when mutex_handle is nullptr
    }
    return 0;
}

DWORD Win32MutexApi::destroy(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept
{
    if (mutex_handle != nullptr) {
        return CloseHandle(mutex_handle);
        mutex_handle = nullptr;
    }
    return 0;
}

DWORD Win32MutexApi::wait_and_acquire(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept
{
    return WaitForSingleObject(mutex_handle, timeout_ms);
}

DWORD Win32MutexApi::release(HANDLE& mutex_handle, LPCWSTR mutex_name) noexcept
{
    return ReleaseMutex(mutex_handle);
}