#include "stdafx.h"
#include "gtest/gtest.h"
#include "splash_controller.cpp"

namespace Oobe
{
    // The tests below only exercise the Idle state. Ideas to enhance testability of this controller are welcome.
    // The whole purpose of adding this state machine technique was to improve overall testability.

    TEST(SplashControllerTests, LaunchFailedShouldStayIdle)
    {
        SplashController controller{"./do_not_exists", GetStdHandle(STD_OUTPUT_HANDLE)};
        controller.sm.addEvent(SplashController::Events::Run{&controller}); // This fails but it is a valid transition.
        ASSERT_TRUE(controller.sm.isCurrentStateA<SplashController::States::Idle>());
    }

    TEST(SplashControllerTests, FailedToFindWindowShouldStayIdle)
    {
        SplashController controller{"cmd.exe", GetStdHandle(STD_OUTPUT_HANDLE)};
        controller.sm.addEvent(SplashController::Events::Run{&controller});
        ASSERT_TRUE(controller.sm.isCurrentStateA<SplashController::States::Idle>());
        controller.sm.addEvent(SplashController::Events::Close{});
        ASSERT_TRUE(controller.sm.isCurrentStateA<SplashController::States::Idle>());
    }

    TEST(SplashControllerTests, OnlyVisibleOrHiddenAcceptsClose)
    {
        SplashController controller{"./do_not_exists", GetStdHandle(STD_OUTPUT_HANDLE)};
        // Only those two states know the window handle to close. Thus this should result in an InvalidTransition.
        ASSERT_FALSE(controller.sm.addEvent(SplashController::Events::Close{}));
        ASSERT_TRUE(controller.sm.isCurrentStateA<SplashController::States::Idle>());
    }
}