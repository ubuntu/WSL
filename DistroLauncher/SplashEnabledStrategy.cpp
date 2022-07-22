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
#include "ApplicationStrategyCommon.h"
#include "SplashEnabledStrategy.h"

namespace Oobe
{
    namespace
    {
        std::filesystem::path splashPath()
        {
            return Win32Utils::thisAppRootdir() / L"ubuntu_wsl_splash.exe";
        }
    } // namespace.

    SplashEnabledStrategy::SplashEnabledStrategy() : splashExePath{splashPath()}
    { }

    void SplashEnabledStrategy::do_run_splash(bool hideConsole)
    {
        if (!std::filesystem::exists(splashExePath)) {
            wprintf(L"Splash executable [%s] not found.\n", splashExePath.c_str());
            return;
        }

        auto now = std::chrono::system_clock::now().time_since_epoch();
        auto pipe = Win32Utils::makeNamedPipe(true, false, std::to_wstring(now.count()));
        if (!pipe.has_value()) {
            wprintf(L"Unable to prepare for the execution of the splash. Error: %s.\n", pipe.error().c_str());
            return;
        }

        // Even though pipe will be moved soon, the handle is just a pointer value, it won't change or invalidate after
        // moving to another owner.
        consoleReadHandle = pipe.value().readHandle();
        console.emplace(std::move(pipe.value()));
        splash.emplace(splashExePath, consoleReadHandle, [this]() { do_show_console(); });

        // unlocks automatically by its destructor.
        std::unique_lock<std::timed_mutex> guard{consoleGuard, std::defer_lock};
        using namespace std::chrono_literals;
        constexpr auto tryLockTimeout = 5s;
        if (!guard.try_lock_for(tryLockTimeout)) {
            wprintf(L"Failed to lock console state for modification. Somebody else is holding the lock.\n");
            return;
        }

        console.value().redirectConsole();
        auto transition = splash.value().sm.addEvent(SplashController<>::Events::Run{&(splash.value())});
        if (!transition.has_value()) {
            // rollback.
            console.value().restoreConsole();
            return;
        }
        if (!std::holds_alternative<SplashController<>::States::Visible>(transition.value())) {
            // also rollback.
            console.value().restoreConsole();
            return;
        }
        splashWindow = std::get<SplashController<>::States::Visible>(transition.value()).window;
        if (hideConsole) {
            consoleIsVisible = !console.value().hideConsoleWindow();
        }
        splashIsRunning = true;
    }

    void SplashEnabledStrategy::do_toggle_splash()
    {
        if (splash.has_value()) {
            splash.value().sm.addEvent(SplashController<>::Events::ToggleVisibility{});
        }
    }

    void SplashEnabledStrategy::do_show_console()
    {
        if (!console.has_value()) {
            return;
        }
        std::unique_lock<std::timed_mutex> guard{consoleGuard, std::defer_lock};
        using namespace std::chrono_literals;
        constexpr auto tryLockTimeout = 5s;
        if (!guard.try_lock_for(tryLockTimeout)) {
            wprintf(L"Failed to lock console state for modification. Somebody else is holding the lock.\n");
            return;
        }
        console.value().restoreConsole();
        HWND topWindow = nullptr;
        if (splashWindow.has_value()) {
            topWindow = splashWindow.value();
        }
        if (!consoleIsVisible) {
            consoleIsVisible = console.value().showConsoleWindow(topWindow);
        }
    }

    void SplashEnabledStrategy::do_close_splash()
    {
        do_show_console();
        if (splashIsRunning) {
            splash.value().sm.addEvent(SplashController<>::Events::Close{&(splash.value())});
            splashIsRunning = false;
            splashWindow.reset();
        }
    }

    HRESULT SplashEnabledStrategy::do_install(Mode uiMode)
    {
        std::array<InstallerController<>::Event, 3> eventSequence{
          InstallerController<>::Events::InteractiveInstall{uiMode}, InstallerController<>::Events::StartInstaller{},
          InstallerController<>::Events::BlockOnInstaller{}};
        HRESULT hr = E_NOTIMPL;
        for (auto& event : eventSequence) {
            auto ok = installer.sm.addEvent(event);

            // unexpected transition occurred here?
            if (!ok.has_value()) {
                do_close_splash();
                return hr;
            }

            std::visit(internal::overloaded{
                         [&](InstallerController<>::States::PreparedTui&) { do_show_console(); },
                         [&](InstallerController<>::States::Ready&) { do_toggle_splash(); },
                         [&](InstallerController<>::States::Success&) {
                             do_close_splash();
                             hr = S_OK;
                         },
                         [&](InstallerController<>::States::UpstreamDefaultInstall& state) {
                             do_show_console();
                             hr = state.hr;
                         },
                         [&](auto&&...) { hr = E_UNEXPECTED; },
                       },
                       ok.value());
        }

        return hr;
    }

    HRESULT SplashEnabledStrategy::do_reconfigure()
    {
        return Oobe::internal::reconfigure_linux_ui(installer);
    }

    HRESULT SplashEnabledStrategy::do_autoinstall(const std::filesystem::path& autoinstall_file)
    {
        return Oobe::internal::do_autoinstall(installer, autoinstall_file);
    }

}
