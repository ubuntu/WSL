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
                        return *this; // return the same (Closed) state.
                    }

                    HWND window = Strategy::do_find_window_by_thread_id(controller->procInfo.dwThreadId);
                    if (window == nullptr) {
                        return *this;
                    }

                    return Visible{window};
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
