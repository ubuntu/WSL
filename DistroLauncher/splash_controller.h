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
#include "state_machine.h"
#include <filesystem>
#include <iostream>

namespace Oobe
{

    namespace internal
    {
        /// Returns the top level window of class [windowClass] of the thread [threadId]
        HWND find_main_thread_window(DWORD threadId, const wchar_t* windowClass);
    }

    // Groups a set of functions of stateless functions that interact with the OS to create a process and manipulate
    // windows according to the needs of the SplashController.
    // Although those functions could be inside the body of the SplashController state transition methods, abstracting
    // out the strategy benefits separation of the OS interaction with the logic of the states, thus benefits
    // testability. Without that separation, the splash controller would never leave the Closed state during tests.
    // Since most of the functions herein implemented are quite small, the class definition is placed in the header
    // aiming to allow compiler see completely through it and ensure optimizing out function calls.
    struct SplashStrategy
    {
        static bool do_create_process(const std::filesystem::path& exePath,
                                      STARTUPINFO& startup,
                                      PROCESS_INFORMATION& process)
        {
            if (!std::filesystem::exists(exePath)) {
                std::wcerr << L"Executable <" << exePath << L"> doesn't exist.\n";
                return false;
            }
            TCHAR szCmdline[MAX_PATH];
            wcsncpy_s(szCmdline, exePath.wstring().c_str(), exePath.wstring().length());
            BOOL res = CreateProcess(nullptr,               // command line
                                     szCmdline,             // non-const CLI
                                     nullptr,               // process security attributes
                                     nullptr,               // primary thread security attributes
                                     TRUE,                  // handles are inherited
                                     0,                     // creation flags
                                     nullptr,               // use parent's environment
                                     nullptr,               // use parent's current directory
                                     &startup,              // STARTUPINFO pointer
                                     &process);             // output: PROCESS_INFORMATION
            return res != 0 && process.hProcess != nullptr; // success
        }

        static HWND SplashStrategy::do_read_window_from_ipc()
        {
            // The following trick is borrowed from Go. unique_ptr must invoke its `deleter` function when it goes out
            // of scope. Instantiating a unique_ptr as automatic variable means it lives in the stack frame of the
            // current function. So, the line `defer pipeCleaner(pipe, &CloseHandle);` means that, for whatever means
            // this function ends, being whatever return path or exception thrown, the function `CloseHandle` will be
            // invoked effectively closing the `HANDLE pipe` variable. That is only possible because Win32 HANDLE is a
            // pointer type (typically void*). This using statement is just a syntactic sugar.
            using defer = std::unique_ptr<std::remove_pointer<HANDLE>::type, decltype(&::CloseHandle)>;

            constexpr int connectionTimeout = 5000; // ms.
            const wchar_t* pipeName{L"\\\\.\\pipe\\Flutter_HWND_Pipe"};
            // CreateThePipe
            SECURITY_ATTRIBUTES pipeSecurity{sizeof(pipeSecurity), nullptr, true};
            HANDLE pipe = CreateNamedPipe(pipeName,
                                          PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                          PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                          PIPE_UNLIMITED_INSTANCES,
                                          0,
                                          0,
                                          0,
                                          &pipeSecurity);
            if (pipe == INVALID_HANDLE_VALUE) {
                std::wcerr << L"Failed to create pipe <" << pipeName << L">.\n";
                return nullptr;
            }
            // One can read this as `defer(CloseHandle(pipe));`
            defer pipeCleaner(pipe, &CloseHandle);
            SetHandleInformation(pipe, HANDLE_FLAG_INHERIT, 1);

            // Setup the wait for connection until timeout.
            OVERLAPPED sync;
            ZeroMemory(&sync, sizeof(sync));
            sync.hEvent = CreateEvent(NULL, TRUE, FALSE, nullptr);
            if (sync.hEvent == INVALID_HANDLE_VALUE || sync.hEvent == 0) {
                std::wcerr << L"Failed to create internal event to notify splash connection\n";
                return nullptr;
            }
            // `defer(CloseHandle(sync.hEvent));`
            defer eventCleaner(sync.hEvent, &CloseHandle);
            if (ConnectNamedPipe(pipe, &sync) == FALSE && GetLastError() != ERROR_PIPE_CONNECTED &&
                GetLastError() != ERROR_IO_PENDING) {
                std::wcerr << L"Failed to connect to splash process\n";
                return nullptr;
            }
            // Block this thread until [connectionTimeout] or the OS notifies the named pipe had a client connected
            // through the event [sync.hEvent].
            if (WaitForSingleObject(sync.hEvent, connectionTimeout) != 0) {
                std::wcerr << L"Timeout: no response from the splash process\n";
                return nullptr;
            }

            // Client connected. Time to read.
            HWND window = nullptr;
            void* lpBuffer{&window};
            DWORD bytesRead = -1;
            BOOL readSuccess = FALSE;
            constexpr DWORD bytesExpected = sizeof(window);
            do {
                readSuccess = ReadFile(pipe, lpBuffer, bytesExpected, &bytesRead, nullptr);
            } while (readSuccess != FALSE && bytesExpected > bytesRead);

            if (bytesExpected != bytesRead) {
                std::wcerr << L"Unexpected value read from the splash process:" << window << L'\n';
                return nullptr;
            }
            return window;
        }

        static HWND do_find_window_by_thread_id(DWORD threadId)
        {
            // Without implementing more sophisticated IPC, it can be tricky to monitor the Flutter process to
            // determine the moment in which the window is ready. Yet, Flutter apps are fast to start up on
            // Windows, so sleeping for some fraction of a second should be enough. The exact amount of time to
            // sleep might be subject to changes after further testing on situations of heavier workloads or
            // slower machines or VM's. Yet, it's expected to stay under less than a second.
            constexpr DWORD flutterWindowToOpenTimeout = 500; // ms.
            Sleep(flutterWindowToOpenTimeout);
            return internal::find_main_thread_window(threadId, L"FLUTTER_RUNNER_WIN32_WINDOW");
        }

        static bool do_show_window(HWND window)
        {
            // If the window was previously visible, the return value is nonzero.
            // If the window was previously hidden, the return value is zero.
            return ShowWindow(window, SW_SHOWNA) == 0;
        }

        static bool do_hide_window(HWND window)
        {
            return ShowWindow(window, SW_HIDE) != 0;
        }

        static bool do_place_behind(HWND toBeFront, HWND toBeBehind)
        {
            return SetWindowPos(toBeBehind, toBeFront, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE) != 0;
        }
        // In the visible state, closing the window should allow the user to confirm closing or doing some clean
        // up. That is the semantic difference between WM_CLOSE (enables that behavior) versus WM_QUIT (closes
        // the window unconditionally). Interestingly, a GUI application that implements that semantics (think
        // of document editor showing a dialog asking if the user wants to save their changes) will hang forever
        // if the window receives WM_CLOSE while hidden, because the OS will not bring it back to the light,
        // thus there is no way the user can hit any of the dialog buttons to let the window rest in peace.
        static void do_forcebly_close(HWND window)
        {
            PostMessage(window, WM_QUIT, 0, 0);
        }

        static void do_gracefully_close(HWND window)
        {
            PostMessage(window, WM_CLOSE, 0, 0);
        }
    }; // struct SplashStrategy

    /// Implements the states, events and control of the splash application. Most of the actual work that requires
    /// interacting with the operating system has been moved out to a strategy class, which by default is SplashStrategy
    /// defined above.
    ///
    /// SplashController admits the following states:
    ///
    /// Closed - not running, initial state upon startup.
    /// Visible - running and displaying the window.
    /// Hidden - running but not displaying the window.
    /// ShouldBeClosed - posted a command to close the splash window. In this state the controller becomes useless.
    ///
    /// For sake of simplicity it's assumed that once the splash window receives the closing command it will perform
    /// certain actions that may take a few seconds and will gently exit, thus no IPC or any other check is implemented
    /// to make sure the window has closed and the application exited.
    ///
    /// The expected state transitions are as follows (using PlantUML notation syntax):
    ///
    /// [*] --> Closed
    /// Closed --> Visible          : Events::Run
    ///
    /// Visible --> Hidden          : Events::ToggleVisibility
    /// Visible --> ShouldBeClosed  : Events::Close
    ///
    /// Hidden --> Visible          : Events::ToggleVisibility
    /// Hidden --> ShouldBeClosed   : Events::Close
    ///
    /// Since ShouldBeClosed state cannot handle any event, it's expected that the Close event will happen only once.
    /// Also, since the Run event can only be handled by the initial state, that event is expected to succeed only once.
    template <typename Strategy = SplashStrategy> class SplashController
    {
      private:
        const std::filesystem::path exePath;
        STARTUPINFO startInfo;
        // this is the only member that requires ownership/resource management since it contains handles to the process
        // that will be created and its threads and that must be closed at some point.
        PROCESS_INFORMATION procInfo;

      public:
        /// Initializes the control structures to later run the splash application. Accepts the splash executable
        /// file system path and the HANDLE to the device that will act as the splash standard input.
        SplashController(std::filesystem::path exePath, not_null<HANDLE> stdIn) : exePath{std::move(exePath)}
        {
            // Most likely the exePath will be built on the fly from a string, so there is no sense in creating a
            // temporary and copy it when we can move.
            ZeroMemory(&procInfo, sizeof(PROCESS_INFORMATION));
            ZeroMemory(&startInfo, sizeof(STARTUPINFO));
            startInfo.cb = sizeof(STARTUPINFO);
            startInfo.hStdInput = stdIn;
            startInfo.dwFlags |= STARTF_USESTDHANDLES;
            SetHandleInformation(stdIn, HANDLE_FLAG_INHERIT, 1);
        }
        // controller events;
        struct Events
        {
            // Run event transports a non-owning mutating (i.e. borrowing) pointer to the outer controller. Using the
            // pointer instead of a reference allows copy assignment, which in turn simplifies passing the event object
            // around by value. With this pointer the Run event will allow the Closed state to access the internals of
            // the controller to find the pieces required to launch the splash application, thus avoiding transferring
            // much heavier pieces of data. Coupling in this case should not be a concern. They are tightly coupled by
            // definition, since all states and events are members of the outer class, thus all can access even the
            // controller's private members.
            struct Run
            {
                not_null<SplashController<Strategy>*> controller;
            };
            struct ToggleVisibility
            { };

            struct PlaceBehind
            {
                HWND front;
            };

            struct Close
            { };
            using EventVariant = std::variant<ToggleVisibility, Run, PlaceBehind, Close>;
        };

        // controller states;
        struct States
        {
            // Forward-declaring the states enables declaring the variant upfront because Closed::run() needs the
            // variant already declared.
            struct Closed;
            struct Visible;
            struct Hidden;
            struct ShouldBeClosed;
            using StateVariant = std::variant<Closed, Visible, Hidden, ShouldBeClosed>;

            struct Closed
            {
                /// Returns a Visible state if it succeeds in:
                /// - launching the splash application and
                /// - finding it's window handle.
                /// Returns itself (i.e. an instance of the Closed state) otherwise.
                StateVariant on_event(typename Events::Run event)
                {
                    auto controller = event.controller;
                    if (!Strategy::do_create_process(
                          controller->exePath, controller->startInfo, controller->procInfo)) {
                        std::wcerr << L"Failed to create the splash process\n";
                        return *this; // return the same (Closed) state.
                    }
                    if (HWND window = Strategy::do_read_window_from_ipc(); window != nullptr) {
                        return Visible{window};
                    }

                    // Fall back in case IPC fails. The Flutter app will be allowed to keep running if its wait for the
                    // pipe times out, assuming some fall back method will be in place.
                    if (HWND window = Strategy::do_find_window_by_thread_id(controller->procInfo.dwThreadId);
                        window != nullptr) {
                        return Visible{window};
                    }
                    std::wcerr << L"Could not find the splash window for Process ID " << event.controller->procInfo.dwProcessId
                               << '\n';
                    return *this;
                }
            };

            // `ShouldBeClosed` because we'll not stop to check whether it really closed when PostMessage returns.
            struct ShouldBeClosed
            { };
            struct Hidden
            {
                HWND window = nullptr;
                Visible on_event(typename Events::ToggleVisibility /*unused*/)
                {
                    Strategy::do_show_window(window);
                    return Visible{window};
                }

                /// Brings the splash window visible but changes its Z-order to stay behind a window specified by the
                /// PlaceBehind event.
                Visible on_event(typename Events::PlaceBehind event)
                {
                    Strategy::do_show_window(window);
                    Strategy::do_place_behind(event.front, window);
                    return Visible{window};
                }

                ShouldBeClosed on_event(typename Events::Close /*unused*/) const
                {
                    Strategy::do_forcebly_close(window);
                    return ShouldBeClosed{};
                }
            };

            struct Visible
            {
                HWND window = nullptr;
                Hidden on_event(typename Events::ToggleVisibility /*unused*/)
                {
                    Strategy::do_hide_window(window);
                    return Hidden{window};
                }

                ShouldBeClosed on_event(typename Events::Close /*unused*/) const
                {
                    Strategy::do_gracefully_close(window);
                    return ShouldBeClosed{};
                }
            };
        };

        // The state machine expects the symbol `State` to be visible.
        // The repetition of `typename` is an unfortunate side effect of having this controller as a template. All event
        // and state types are now dependent and cannot be explicitly referred to without the templated type argument.
        using State = typename States::StateVariant;
        using Event = typename Events::EventVariant;
        internal::state_machine<SplashController<Strategy>> sm;

        ~SplashController()
        {
            if (procInfo.hProcess != nullptr) {
                TerminateProcess(procInfo.hProcess, 0);
                CloseHandle(procInfo.hThread);
                CloseHandle(procInfo.hProcess);
            }
        }
    };
}
