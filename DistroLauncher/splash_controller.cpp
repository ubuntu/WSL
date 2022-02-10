/*
 * Copyright (C) 2021 Canonical Ltd
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

        HWND find_main_thread_window(DWORD thread_id)
        {
            window_data data{L"FLUTTER_RUNNER_WIN32_WINDOW", nullptr};
            EnumThreadWindows(thread_id, enum_windows_callback, reinterpret_cast<LPARAM>(&data));
            return data.window_handle;
        }
    }

    // Most likely the exePath will be built on the fly from a string, so there is no sense in creating a temporary and
    // copy it when we can move.
    SplashController::SplashController(std::filesystem::path exePath, not_null<HANDLE> stdIn) :
        exePath{std::move(exePath)}
    {
        ZeroMemory(&_piProcInfo, sizeof(PROCESS_INFORMATION));
        ZeroMemory(&startInfo, sizeof(STARTUPINFO));
        startInfo.cb = sizeof(STARTUPINFO);
        startInfo.hStdInput = stdIn;
        startInfo.dwFlags |= STARTF_USESTDHANDLES;
        SetHandleInformation(stdIn, HANDLE_FLAG_INHERIT, 1);
    }

    SplashController::State SplashController::States::Idle::on_event(Events::Run event)
    {
        TCHAR szCmdline[MAX_PATH];
        wcsncpy_s(
          szCmdline, event.controller_->exePath.wstring().c_str(), event.controller_->exePath.wstring().length());
        BOOL res = CreateProcess(nullptr,                          // command line
                                 szCmdline,                        // non-const CLI
                                 nullptr,                          // process security attributes
                                 nullptr,                          // primary thread security attributes
                                 TRUE,                             // handles are inherited
                                 0,                                // creation flags
                                 nullptr,                          // use parent's environment
                                 nullptr,                          // use parent's current directory
                                 &event.controller_->startInfo,    // STARTUPINFO pointer
                                 &event.controller_->_piProcInfo); // output: PROCESS_INFORMATION
        if (res == 0 || event.controller_->_piProcInfo.hProcess == nullptr) {
            return *this; // return the the same (Idle) state.
        }
        // Without implementing more sofisticated IPC, it can be tricky to monitor the Flutter process to determine the
        // moment in which the window is ready. Yet, Flutter apps are fast to start up on Windows, so sleeping for some
        // fraction of a second should be enough. The exact amount of time to sleep might be subject to changes after
        // further testing on situations of heavier workloads or slower machines or VM's. Yet, it's expected to stay
        // under less than a second.
        constexpr DWORD flutterWindowToOpenTimeout = 500; // ms.
        Sleep(flutterWindowToOpenTimeout);
        HWND window = internal::find_main_thread_window(event.controller_->_piProcInfo.dwThreadId);
        if (window == nullptr) {
            return *this;
        }

        return Visible{window};
    }

    // The Close event should be used to gracefully destroys the flutter app.
    // This will just kill the process unconditionally.
    SplashController::~SplashController()
    {
        if (_piProcInfo.hProcess != nullptr) {
            TerminateProcess(_piProcInfo.hProcess, 0);
            CloseHandle(_piProcInfo.hThread);
            CloseHandle(_piProcInfo.hProcess);
        }
    }

}
