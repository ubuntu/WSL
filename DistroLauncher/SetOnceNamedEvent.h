#pragma once
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
namespace Win32Utils
{
    /*
     *  Abstract
     *  --------
     *  An abstraction on top of Win32 Named Events that can be set only once.
     *
     *  Usage
     *  --------
     *  Ensure a unique name is used for its initialization. This is an IPC mechanism, so other processes will find it.
     *
     *  ```
     *  SetOnceNamedEvent event(L"unique_identifyier");
     *  ```
     *  When something interesting happens and you want to let the world know about it, set the event:
     *
     *  ```
     *  bool ok = event.set();
     *  ```
     *
     *  That's it. From now on the variable is useless. The event remains valid from a succesfull construction until
     * it's set.
     *
     * ```
     * ASSERT_FALSE(event.isValid())
     * ````
     *
     * NOTE: Win32 Events (named or unnamed) are resetable, meaning that one can toggle on-off as much as desired. They
     * can even be automatically reset by the OS, if configured to do so by the CreateEvent function. This class
     * restricts that behavior by closing the OS handle and nulling out the private field that would represent it, thus
     * rendering the object useless after being once set.
     */
    class SetOnceNamedEvent
    {
      public:
        // Creates an event called [name].
        explicit SetOnceNamedEvent(const wchar_t* name) noexcept;
        ~SetOnceNamedEvent() noexcept;
        SetOnceNamedEvent(SetOnceNamedEvent&& other) noexcept;
        SetOnceNamedEvent(const SetOnceNamedEvent& other) = delete;
        SetOnceNamedEvent& operator=(const SetOnceNamedEvent& other) = delete;
        // Sets the event and subsequently closes its handle. From this moment on the object becomes useless.
        // Returns false on failure. NOTE: Subsequent calls are no-op.
        [[nodiscard]] bool set() noexcept;
        [[nodiscard]] bool isValid() const noexcept;

      private:
        HANDLE event = nullptr;
        bool alreadySet = false;
    };
}
