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

#pragma once
#include "state_machine.h"
#include <filesystem>

namespace Oobe
{
    class SplashController
    {
      public:
        /// Initializes the control structures to later run the splash application. Accepts the splash executable
        /// filesystem
        /// path and the HANDLE to the device that will act as the splash standard input.
        SplashController(std::filesystem::path exePath, not_null<HANDLE> stdIn);
        // controller events;
        struct Events
        {
            // Run event transports a non-owning mutating (i.e. borrowing) pointer to the outer controller. Using the
            // pointer instead of a reference allows copy assignment, which in turn simplifies passing the event object
            // around by value. With this pointer the Run event will allow the Idle state to access the internals of the
            // controller to find the pieces required to launch the splash applicaiton, thus avoiding transferring much
            // heavier pieces of data. Coupling in this case should not be a concern. They are tightly coupled by
            // definition, since all states and events are members of the outer class, thus all can access even the
            // controller's private members.
            struct Run
            {
                not_null<SplashController*> controller_;
            };
            struct ToggleVisibility
            { };

            struct PlaceBehind
            {
                HWND front;
            };

            struct Close
            { };
        };
        using Event = std::variant<Events::ToggleVisibility, Events::Run, Events::PlaceBehind, Events::Close>;

        // controller states;
        struct States
        {
            // Forward-declaring the states enables declaring the variant upfront. Idle::run() needs the variant
            // already declared.
            struct Visible;
            struct Hidden;
            struct Idle;
            struct ShouldBeClosed;
            using StateVariant = std::variant<Idle, Visible, Hidden, ShouldBeClosed>;

            struct Idle
            {
                /// Returns a Visible state if it succeeds in:
                /// - launching the splash application and
                /// - finding it's window handle.
                /// Returns itself (i.e. an instance of the Idle state) otherwise.
                StateVariant on_event(Events::Run event);
            };
            // `ShouldBeClosed` because we'll not stop to check whether it really closed when PostMessage returns.
            struct ShouldBeClosed
            { };
            struct Hidden
            {
                HWND window = nullptr;
                Visible on_event(Events::ToggleVisibility /*unused*/)
                {
                    ShowWindow(window, SW_SHOWNA);
                    return Visible{window};
                }

                /// Brings the splash window visible but changes its Z-order to stay behind a window specified by the
                /// PlaceBehind event.
                Visible on_event(Events::PlaceBehind event)
                {
                    ShowWindow(window, SW_SHOWNA);
                    SetWindowPos(window, event.front, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                    return Visible{window};
                }
                // In the visible state, closing the window should allow the user to confirm closing or doing some clean
                // up. That is the semantic difference between WM_CLOSE (enables that behavior) versus WM_QUIT (closes
                // the window unconditionally). Interestingly, a GUI application that implements that semantics (think
                // of document editor showing a dialog asking if the user wants to save their changes) will hang forever
                // if the window receives WM_CLOSE while hidden, because the OS will not bring it back to the light,
                // thus there is no way the user can hit any of the dialog buttons to let the window rest in piece.
                ShouldBeClosed on_event(Events::Close event) const
                {
                    PostMessage(window, WM_QUIT, 0, 0);
                    return ShouldBeClosed{};
                }
            };

            struct Visible
            {
                HWND window = nullptr;
                Hidden on_event(Events::ToggleVisibility /*unused*/)
                {
                    ShowWindow(window, SW_HIDE);
                    return Hidden{window};
                }

                ShouldBeClosed on_event(Events::Close event) const
                {
                    PostMessage(window, WM_CLOSE, 0, 0);
                    return ShouldBeClosed{};
                }
            };
        };
        // The state machine expects the symbol `State` to be visible.
        using State = States::StateVariant;

        internal::state_machine<SplashController> sm;

        ~SplashController();

      private:
        const std::filesystem::path exePath;
        STARTUPINFO startInfo;
        // this is the only member that requires ownership/resource management since it contains handles to the process
        // that will be created and its threads and that must be closed at some point.
        PROCESS_INFORMATION _piProcInfo;
    };

}