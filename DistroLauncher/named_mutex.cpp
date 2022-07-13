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

DWORD NamedMutex::create() noexcept
{
    mutex_handle = CreateMutex(nullptr, FALSE, mutex_name.c_str());
    if (mutex_handle == nullptr) {
        const auto error = GetLastError();
        return error != 0 ? error
                          : ~static_cast<DWORD>(0); // Ensuring return value is always != 0 when mutex_handle is nullptr
    }
    return 0;
}

DWORD NamedMutex::destroy() noexcept
{
    if (static_cast<bool>(mutex_handle)) {
        return CloseHandle(mutex_handle);
    }
    return 0;
}

DWORD NamedMutex::wait_and_acquire() noexcept
{
    if (!mutex_handle) {
        DWORD success = create();
        if (success != 0) {
            return success;
        }
    }
    __assume(static_cast<bool>(mutex_handle));
    return WaitForSingleObject(mutex_handle, timeout_ms);
}

DWORD NamedMutex::release() noexcept
{
    return ReleaseMutex(mutex_handle);
}