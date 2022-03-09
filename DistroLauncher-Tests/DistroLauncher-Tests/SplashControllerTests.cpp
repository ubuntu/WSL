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
#include "splash_controller.h"

namespace Oobe
{
    const auto* fakeFileName = L"./do_not_exists";
    // I just need to compare to nullptr, I'll not do anything else with that pointer.
    HWND globalFakeWindow = reinterpret_cast<HWND>(const_cast<wchar_t*>(fakeFileName));
    // Fake strategies to exercise the Splash controller state machine.
    struct NothingWorksStrategy
    {
        static bool do_create_process(const std::filesystem::path& exePath,
                                      STARTUPINFO& startup,
                                      PROCESS_INFORMATION& process)
        {
            return false;
        }

        static HWND do_read_window_from_ipc()
        {
            return nullptr;
        }

        static HWND do_find_window_by_thread_id(DWORD threadId)
        {
            return nullptr;
        }
        static HANDLE do_on_close(HANDLE process, WAITORTIMERCALLBACK callback, void* data)
        {
            return nullptr;
        }
        static void do_cleanup_process(PROCESS_INFORMATION& h)
        { }
        static void do_unsubscribe(HANDLE h)
        { }
        // The other methods will never be called, so there is no need to define them. Otherwise it would not even
        // compile.
    }; // struct NothingWorksStrategy

    struct EverythingWorksStrategy
    {
        static bool do_create_process(const std::filesystem::path& exePath,
                                      STARTUPINFO& startup,
                                      PROCESS_INFORMATION& process)
        {
            return true;
        }

        static HWND do_read_window_from_ipc()
        {
            // no risk because this handle will not be used for anything besides passing around.
            return globalFakeWindow;
        }

        static HWND do_find_window_by_thread_id(DWORD threadId)
        {
            // no risk because this handle will not be used for anything besides passing around.
            return globalFakeWindow;
        }
        static bool do_show_window(HWND window)
        {
            return true;
        }
        static bool do_hide_window(HWND window)
        {
            return true;
        }
        static bool do_place_behind(HWND toBeFront, HWND toBeBehind)
        {
            return true;
        }

        static void do_gracefully_close(HWND window)
        { }

        static void do_cleanup_process(PROCESS_INFORMATION& h)
        { }

        static HANDLE do_on_close(HANDLE process, WAITORTIMERCALLBACK callback, void* data)
        {
            return static_cast<HANDLE>(globalFakeWindow);
        }

        static void do_unsubscribe(HANDLE h)
        { }
        // The other methods will never be called, so there is no need to define them. Otherwise it would not even
        // compile.
    }; // struct EverythingWorksStrategy

    auto callback = []() {};

    // The whole purpose of adding this state machine technique was to improve overall testability.
    TEST(SplashControllerTests, LaunchFailedShouldStayIdle)
    {
        using Controller = SplashController<NothingWorksStrategy>;
        Controller controller{fakeFileName, GetStdHandle(STD_OUTPUT_HANDLE), callback};
        controller.sm.addEvent(Controller::Events::Run{&controller}); // This fails but it is a valid transition.
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Closed>());
    }

    TEST(SplashControllerTests, FailedToFindWindowShouldStayIdle)
    {
        struct CantFindWindowStrategy
        {
            static bool do_create_process(const std::filesystem::path& exePath,
                                          STARTUPINFO& startup,
                                          PROCESS_INFORMATION& process)
            {
                return true;
            }

            static HWND do_read_window_from_ipc()
            {
                return nullptr;
            }

            static HWND do_find_window_by_thread_id(DWORD threadId)
            {
                return nullptr;
            }
            static HANDLE do_on_close(HANDLE process, WAITORTIMERCALLBACK callback, void* data)
            {
                return static_cast<HANDLE>(globalFakeWindow);
            }

            static void do_gracefully_close(HWND window)
            { }
            static void do_cleanup_process(PROCESS_INFORMATION& h)
            { }
            static void do_unsubscribe(HANDLE h)
            { }
        }; // struct CantFindWindowStrategy

        using Controller = SplashController<CantFindWindowStrategy>;
        Controller controller{fakeFileName, GetStdHandle(STD_OUTPUT_HANDLE), callback};
        controller.sm.addEvent(Controller::Events::Run{&controller});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Closed>());
        controller.sm.addEvent(Controller::Events::Close{&controller});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Closed>());
    }

    TEST(SplashControllerTests, IPCMustBePreferred)
    {
        struct AlmostEverythingWorksStrategy
        {
            static bool do_create_process(const std::filesystem::path& exePath,
                                          STARTUPINFO& startup,
                                          PROCESS_INFORMATION& process)
            {
                return true;
            }

            static HWND do_read_window_from_ipc()
            {
                // no risk because this handle will not be used for anything besides passing around.
                return globalFakeWindow;
            }

            static HWND do_find_window_by_thread_id(DWORD threadId)
            {
                EXPECT_TRUE(false) << "This should not be called in this test\n";
                return nullptr;
            }
            static bool do_show_window(HWND window)
            {
                return true;
            }
            static bool do_hide_window(HWND window)
            {
                return true;
            }
            static bool do_place_behind(HWND toBeFront, HWND toBeBehind)
            {
                return true;
            }
            static HANDLE do_on_close(HANDLE process, WAITORTIMERCALLBACK callback, void* data)
            {
                return static_cast<HANDLE>(globalFakeWindow);
            }

            static void do_gracefully_close(HWND window)
            { }
            static void do_cleanup_process(PROCESS_INFORMATION& h)
            { }
            static void do_unsubscribe(HANDLE h)
            { }
        }; // struct AlmostEverythingWorksStrategy

        using Controller = SplashController<AlmostEverythingWorksStrategy>;
        Controller controller{fakeFileName, GetStdHandle(STD_OUTPUT_HANDLE), callback};
        auto transition = controller.sm.addEvent(Controller::Events::Run{&controller});
        // Since almost everything works in this realm, all transitions below should be valid...
        ASSERT_TRUE(transition.has_value());
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Visible>());
    }

    TEST(SplashControllerTests, AHappySequenceOfEvents)
    {
        using Controller = SplashController<EverythingWorksStrategy>;
        Controller controller{fakeFileName, GetStdHandle(STD_OUTPUT_HANDLE), callback};
        auto transition = controller.sm.addEvent(Controller::Events::Run{&controller});
        // Since everything works in this realm, all transitions below should be valid...
        ASSERT_TRUE(transition.has_value());
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Visible>());
        transition = controller.sm.addEvent(Controller::Events::ToggleVisibility{});
        ASSERT_TRUE(transition.has_value());
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Hidden>());
        transition = controller.sm.addEvent(Controller::Events::ToggleVisibility{});
        ASSERT_TRUE(transition.has_value());
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Visible>());
        transition = controller.sm.addEvent(Controller::Events::ToggleVisibility{});
        ASSERT_TRUE(transition.has_value());
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Hidden>());
        transition = controller.sm.addEvent(Controller::Events::PlaceBehind{GetConsoleWindow()});
        ASSERT_TRUE(transition.has_value());
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Visible>());
        transition = controller.sm.addEvent(Controller::Events::Close{&controller});
        ASSERT_TRUE(transition.has_value());
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::ShouldBeClosed>());
    }
    // This proves to be impossible to run the splash application more than once after the first success.
    TEST(SplashControllerTests, OnlyIdleStateAcceptsRunEvent)
    {
        using Controller = SplashController<EverythingWorksStrategy>;
        Controller controller{fakeFileName, GetStdHandle(STD_OUTPUT_HANDLE), callback};
        auto transition = controller.sm.addEvent(Controller::Events::Run{&controller});
        // Since everything works in this realm, all transitions below should be valid...
        ASSERT_TRUE(transition.has_value());
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Visible>());

        // now comes the interesting part: all other states should refuse the Run event

        // Exercising the Visible State.
        transition = controller.sm.addEvent(Controller::Events::Run{&controller});
        ASSERT_FALSE(transition.has_value());
        // state should remain the previous one.
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Visible>());

        transition = controller.sm.addEvent(Controller::Events::ToggleVisibility{});
        ASSERT_TRUE(transition.has_value());
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Hidden>());

        // Exercising the Hidden State.
        transition = controller.sm.addEvent(Controller::Events::Run{&controller});
        ASSERT_FALSE(transition.has_value());
        // state should remain the previous one.
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Hidden>());

        transition = controller.sm.addEvent(Controller::Events::Close{&controller});
        ASSERT_TRUE(transition.has_value());
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::ShouldBeClosed>());
        // Exercising the ShouldBeClosed State. Not even this once accepts re-running the splash.
        transition = controller.sm.addEvent(Controller::Events::Run{&controller});
        ASSERT_FALSE(transition.has_value());
        // state should remain the previous one.
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::ShouldBeClosed>());
    }
    // This proves to be impossible to close the window twice.
    TEST(SplashControllerTests, MustCloseOnlyOnce)
    {
        // Remember that in this realm everything just works...
        using Controller = SplashController<EverythingWorksStrategy>;
        Controller controller{fakeFileName, GetStdHandle(STD_OUTPUT_HANDLE), callback};
        auto transition = controller.sm.addEvent(Controller::Events::Run{&controller});
        ASSERT_TRUE(transition.has_value());
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Visible>());
        transition = controller.sm.addEvent(Controller::Events::Close{&controller});
        ASSERT_TRUE(transition.has_value());
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::ShouldBeClosed>());
        // silly attempts start here.
        transition = controller.sm.addEvent(Controller::Events::ToggleVisibility{});
        ASSERT_FALSE(transition.has_value());
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::ShouldBeClosed>());
        transition = controller.sm.addEvent(Controller::Events::Close{&controller});
        ASSERT_FALSE(transition.has_value()); // if closing twice worked, this assertion would fail.
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::ShouldBeClosed>());

        // since it's not possible to run a second time...
        transition = controller.sm.addEvent(Controller::Events::Run{&controller});
        ASSERT_FALSE(transition.has_value());
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::ShouldBeClosed>());

        // and here we make sure the state machine was not fooled by the attempt to run.
        transition = controller.sm.addEvent(Controller::Events::Close{&controller});
        ASSERT_FALSE(transition.has_value());
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::ShouldBeClosed>());
    }
}