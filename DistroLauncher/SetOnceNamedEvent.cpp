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
#include "SetOnceNamedEvent.h"

namespace Win32Utils
{

    SetOnceNamedEvent::SetOnceNamedEvent(const wchar_t* name) noexcept
    {
        HANDLE handle = CreateEvent(nullptr, TRUE, FALSE, name);
        if (handle != nullptr) {
            event = handle;
            setOnce = false;
        }
    }

    SetOnceNamedEvent::~SetOnceNamedEvent() noexcept
    {
        if (event != nullptr) {
            CloseHandle(event);
            event = nullptr;
            setOnce = false;
        }
    }

    bool SetOnceNamedEvent::set() noexcept
    {
        if (setOnce == true) {
            return false;
        }

        if (SignalObjectAndWait(event, event, INFINITE, FALSE) != WAIT_OBJECT_0) {
            return false;
        }

        setOnce = true;
        return true;
    }

    SetOnceNamedEvent::SetOnceNamedEvent(SetOnceNamedEvent&& other) noexcept :
        event{std::exchange(other.event, nullptr)}, setOnce{std::exchange(other.setOnce, false)}
    {
    }

    bool SetOnceNamedEvent::isValid() const noexcept
    {
        return event != nullptr && setOnce == false;
    }
}
