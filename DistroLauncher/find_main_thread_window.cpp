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
#include "splash_controller.h"

namespace Oobe
{

    namespace internal
    {
        // The struct and three functions below work very deeply integrated to implement one Win32 idiom required to
        // find the splash screen window handle, which the splash controller will need to do the control job. From
        // bottom up:
        // 1. find_main_thread_window - uses the Win32 EnumThreadWindows to iterate over all windows created by a
        //    specific thread, calling enum_windows_callback for each window found, passing the window handle and a
        //    void* (used to pass arbitrary data to the callback).
        // 2. enum_windows_callback is called by the OS with the window handle and that void* called lParam to check if
        //    matches the window we are looking for.
        //    We use this lParam "channel" as a two way: we pass to the callback the window class name as
        //    input and receive the Window Handle as an output. That's why the struct window_data was created, to group
        //    both pieces of information together and allow accessing both thru a single pointer. The return channel of
        //    that callback is already occupied with the information the OS expects to know whether it must stop the
        //    iteration or not. Returning FALSE means we found it.
        // 3. is_main_window - is just an extra sanity check to make sure the window we are looking into is a top one (
        //    not a child MDI or window control).
        //
        // Having this pattern only searching for Windows created by a specific thread, we don't need to search for the
        // window name, for instance. Matching the window class name suffices. Bear in mind that the Flutter app changes
        // its window name based on the language, so comparing the window name would be really tricky.
        //
        // Also, this idiom only works for windows created by the thread whose ID is informed. Finding a console window
        // from a child process of the console is not possible with this approach, because the look up behavior goes
        // from the thread to the window and in the example of the console the window would exist prior to the creation
        // of the application and its threads.
        struct window_data
        {
            const wchar_t* szClassName;
            HWND window_handle = nullptr;
        };

        // Sanity check because everything is a Window on Windows. By everything, I mean controls, buttons etc etc.
        BOOL is_main_window(HWND handle)
        {
            return static_cast<BOOL>(GetWindow(handle, GW_OWNER) == nullptr && (IsWindowVisible(handle) != 0));
        }

        BOOL CALLBACK enum_windows_callback(HWND handle, LPARAM lParam)
        {
            constexpr auto maxSize = 32;
            // NOLINTNEXTLINE(performance-no-int-to-ptr) - The API doesn't give us options.
            window_data* data = reinterpret_cast<window_data*>(lParam);
            wchar_t windowClass[maxSize];
            GetClassName(handle, windowClass, maxSize);
            std::wstring_view klass{windowClass};
            // Sanity check to make sure we found the correct window. It seems very unlikely not to be the correct one,
            // but this small check decreases quite dramatically chance of mistakes.
            // If not the window i'm looking for, keep iterating...
            if ((is_main_window(handle) == 0) || klass.compare(data->szClassName) != 0) {
                return TRUE;
            }
            // else save the handle.
            data->window_handle = handle;
            return FALSE;
        }

        HWND find_main_thread_window(DWORD thread_id, const wchar_t* windowClass)
        {
            window_data data{windowClass, nullptr};
            EnumThreadWindows(thread_id, enum_windows_callback, reinterpret_cast<LPARAM>(&data));
            return data.window_handle;
        }
    }
}
