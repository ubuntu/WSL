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
#include "InstallerControllerTestPolicies.h"
#include "installer_controller.h"

namespace Oobe
{

    // Those ensure older versions of the distro are protected from invoking OOBE if missing.
    TEST(InstallerControllerTests, UpstreamIfMissingOobeOnAutoInstall)
    {
        using Controller = InstallerController<NothingWorksPolicy>;
        Controller controller{};
        controller.sm.addEvent(Controller::Events::AutoInstall{L"./"});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::UpstreamDefaultInstall>());
    }
    TEST(InstallerControllerTests, UpstreamIfMissingOobeOnReconfig)
    {
        using Controller = InstallerController<NothingWorksPolicy>;
        Controller controller{};
        controller.sm.addEvent(Controller::Events::Reconfig{});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::UpstreamDefaultInstall>());
    }
    TEST(InstallerControllerTests, UpstreamIfMissingOobeOnInteractive)
    {
        using Controller = InstallerController<NothingWorksPolicy>;
        Controller controller{};
        controller.sm.addEvent(Controller::Events::InteractiveInstall{});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::UpstreamDefaultInstall>());
    }

    // Ensures the controller is useless if in UpstreamDefaultInstall state.
    TEST(InstallerControllerTests, UpstreamStateAcceptsNoEvent)
    {
        using Controller = InstallerController<NothingWorksPolicy>;
        Controller controller{};
        controller.sm.addEvent(Controller::Events::AutoInstall{L"./"});
        EXPECT_TRUE(controller.sm.isCurrentStateA<Controller::States::UpstreamDefaultInstall>());
        std::array<Controller::Event, 5> events{
          Controller::Events::AutoInstall{L"./"},   Controller::Events::Reconfig{},
          Controller::Events::InteractiveInstall{}, Controller::Events::StartInstaller{},
          Controller::Events::BlockOnInstaller{},
        };
        std::for_each(std::begin(events), std::end(events), // apply all events one by one.
                      [&](Controller::Event ev) {
                          // Just to ensure the inner type is passed instead of the variant Event.
                          std::visit(internal::overloaded{[&](auto&& e) {
                                         auto transition = controller.sm.addEvent(e);
                                         ASSERT_FALSE(transition.has_value());
                                     }},
                                     ev);
                      });
    }

    TEST(InstallerControllerTests, HappyAutoInstall)
    {
        using Controller = InstallerController<EverythingWorksPolicy>;
        Controller controller{};
        auto ok = controller.sm.addEvent(Controller::Events::AutoInstall{L"./"});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::AutoInstalling>());
        // introspecting the returned state to make sure auto install is always in text mode.
        auto state = std::get<Controller::States::AutoInstalling>(ok.value());
        ASSERT_TRUE(state.cli.find(L"--text") != std::wstring::npos);
        ok = controller.sm.addEvent(Controller::Events::BlockOnInstaller{});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Success>());
    }
    TEST(InstallerControllerTests, HappyReconfig)
    {
        using Controller = InstallerController<EverythingWorksPolicy>;
        Controller controller{};
        auto ok = controller.sm.addEvent(Controller::Events::Reconfig{});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::PreparedGui>());
        controller.sm.addEvent(Controller::Events::StartInstaller{});
        // opportunity for the application to react and ensure the context is ready for user interaction.
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Ready>());
        controller.sm.addEvent(Controller::Events::BlockOnInstaller{});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Success>());
    }
    TEST(InstallerControllerTests, HappyReconfigTui)
    {
        using Controller = InstallerController<EverythingWorksTuiPolicy>;
        Controller controller{};
        auto ok = controller.sm.addEvent(Controller::Events::Reconfig{});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Success>());
    }
    TEST(InstallerControllerTests, HappyInteractiveInstall)
    {
        using Controller = InstallerController<EverythingWorksPolicy>;
        Controller controller{};
        controller.sm.addEvent(Controller::Events::InteractiveInstall{});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::PreparedGui>());
        controller.sm.addEvent(Controller::Events::StartInstaller{});
        // opportunity for the application to react and ensure the context is ready for user interaction.
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Ready>());
        controller.sm.addEvent(Controller::Events::BlockOnInstaller{});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Success>());
    }

    TEST(InstallerControllerTests, FailToLaunchGoesUpstream)
    {
        using Controller = InstallerController<FailsToLaunchPolicy>;
        Controller controller{};
        controller.sm.addEvent(Controller::Events::InteractiveInstall{});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::PreparedGui>());
        controller.sm.addEvent(Controller::Events::StartInstaller{});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::UpstreamDefaultInstall>());
    }

    TEST(InstallerControllerTests, OobeCrashGoesUpstreamInteractive)
    {
        using Controller = InstallerController<OobeCrashDetectedPolicy>;
        Controller controller{};
        controller.sm.addEvent(Controller::Events::InteractiveInstall{});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::PreparedGui>());
        controller.sm.addEvent(Controller::Events::StartInstaller{});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::Ready>());
        controller.sm.addEvent(Controller::Events::BlockOnInstaller{});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::UpstreamDefaultInstall>());
    }
    TEST(InstallerControllerTests, OobeCrashGoesUpstreamInAuto)
    {
        using Controller = InstallerController<OobeCrashDetectedPolicy>;
        Controller controller{};
        controller.sm.addEvent(Controller::Events::AutoInstall{L"./"});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::AutoInstalling>());
        controller.sm.addEvent(Controller::Events::BlockOnInstaller{});
        ASSERT_TRUE(controller.sm.isCurrentStateA<Controller::States::UpstreamDefaultInstall>());
    }
}