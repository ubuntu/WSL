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
#include "gtest/gtest.h"
#include "state_machine.h"

namespace Oobe
{
    /// This WindowController admits three states:
    /// Idle - not running,
    /// Visible - running and displaying the window and
    /// Hidden - running but not displaying the window.
    ///
    /// And two events, whose names are self explanatory:
    /// Run and ToggleVisibility.
    /// (The events could carry data, but for this example they don't.)
    /// By inspecting the `on_event()` methods of each state class we can draft a state transition table (PlantUML
    /// syntax):
    ///
    /// [*] --> Idle
    /// Idle --> Visible : Events::Run
    /// Visible --> Hidden : Events::ToggleVisibility
    /// Hidden --> Visible : Events::ToggleVisibility
    ///
    /// There are no guards to the transitions themselves, but there are some invalid possibilities (left unhandled by the
    /// state classes):
    /// * Idle cannot receive Events::ToggleVisibility
    /// * Neither Hidden or Visible can receive Events::Run
    ///
    /// While exercising the API, the subsequent test cases also exercise:
    /// TEST(StateMachineTests, ValidTransition) -> handling valid state transitions.
    /// TEST(StateMachineTests, InvalidTransitionShouldBeDiscarded) -> handling invalid transitions.
    struct WindowController
    {
        // controller events;
        struct Events
        {
            struct Run
            { };
            struct ToggleVisibility
            { };
        };
        // controller states;
        struct States
        {
            struct Visible;

            struct Idle
            {
              private:
                Visible run()
                {
                    std::wcerr << L"[IdleState] Running... visible: yes\n";
                    return Visible{42};
                }

              public:
                Visible on_event(Events::Run event)
                {
                    return run();
                }
            };

            struct Hidden
            {
                int window = -1;
                Visible toggle()
                {
                    std::wcerr << L"[HiddenState] Showing...\n";
                    return Visible{window};
                }
                Visible on_event(Events::ToggleVisibility event)
                {
                    return toggle();
                }
            };

            struct Visible
            {
                int window = -1;

                Hidden on_event(Events::ToggleVisibility event)
                {
                    std::wcerr << L"[ShownState] Hiding...\n";
                    return Hidden{window};
                }
            };
        };
        using State = std::variant<States::Idle, States::Visible, States::Hidden>;
        using Event = std::variant<Events::Run, Events::ToggleVisibility>;

        internal::state_machine<WindowController> sm;
    };

    TEST(StateMachineTests, ValidTransition)
    {
        WindowController controller{};
        // Here we pass the events directly yo the addEvent method.
        ASSERT_TRUE(controller.sm.isCurrentStateA<WindowController::States::Idle>());
        WindowController::Event event = WindowController::Events::Run{};
        controller.sm.addEvent(event);
        ASSERT_TRUE(controller.sm.isCurrentStateA<WindowController::States::Visible>());
        controller.sm.addEvent(WindowController::Events::ToggleVisibility{});
        ASSERT_TRUE(controller.sm.isCurrentStateA<WindowController::States::Hidden>());
        controller.sm.addEvent(WindowController::Events::ToggleVisibility{});
        ASSERT_TRUE(controller.sm.isCurrentStateA<WindowController::States::Visible>());
    }

    TEST(StateMachineTests, InvalidTransitionShouldBeDiscarded)
    {
        WindowController controller{};
        // In this example we reuse a Event variant variable and keep reassigning it.
        WindowController::Event event = WindowController::Events::ToggleVisibility{};

        ASSERT_TRUE(controller.sm.isCurrentStateA<WindowController::States::Idle>());
        ASSERT_FALSE(controller.sm.addEvent(event));

        event = WindowController::Events::Run{};
        ASSERT_TRUE(controller.sm.addEvent(event));
        ASSERT_TRUE(controller.sm.isCurrentStateA<WindowController::States::Visible>());

        event = WindowController::Events::ToggleVisibility{};
        ASSERT_TRUE(controller.sm.addEvent(event));
        ASSERT_TRUE(controller.sm.isCurrentStateA<WindowController::States::Hidden>());

        // Since its running, it should not accept a new Run event, state should not change.
        event = WindowController::Events::Run{};
        ASSERT_FALSE(controller.sm.addEvent(event));
        ASSERT_TRUE(controller.sm.isCurrentStateA<WindowController::States::Hidden>());
    }

}