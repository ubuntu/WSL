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

namespace Oobe
{
    namespace
    {
        // Those functions would be complete duplicates if they were methods in both strategy classes.
        HRESULT do_reconfigure(Oobe::InstallerController<>& controller)
        {
            std::array<InstallerController<>::Event, 3> eventSequence{
              InstallerController<>::Events::Reconfig{}, InstallerController<>::Events::StartInstaller{},
              InstallerController<>::Events::BlockOnInstaller{}};

            // for better readability.
            constexpr auto S_CONTINUE = E_NOTIMPL;
            HRESULT hr = S_CONTINUE;

            for (int i = 0; i < eventSequence.size() && hr == S_CONTINUE; ++i) {
                auto ok = controller.sm.addEvent(eventSequence[i]);
                if (!ok) {
                    return hr;
                }

                // We can now have:
                // States::UpstreamDefaultInstall on failure;
                // States::Closed -> States::Success (for text mode) or
                // States::Closed -> States::PreparedGui -> States::Ready -> States::Success (for GUI)
                hr =
                  std::visit(internal::overloaded{
                               [&](InstallerController<>::States::Success&) { return S_OK; },
                               [&](InstallerController<>::States::PreparedGui&) { return S_CONTINUE; },
                               [&](InstallerController<>::States::Ready&) { return S_CONTINUE; },
                               [&](InstallerController<>::States::UpstreamDefaultInstall& state) { return state.hr; },
                               [&](auto&&...) { return E_UNEXPECTED; },
                             },
                             ok.value());
            }
            return hr;
        }

        HRESULT do_autoinstall(InstallerController<>& controller, const std::filesystem::path& autoinstall_file)
        {
            auto& stateMachine = controller.sm;

            auto ok = stateMachine.addEvent(InstallerController<>::Events::AutoInstall{autoinstall_file});
            if (!ok) {
                return E_FAIL;
            }
            ok = stateMachine.addEvent(InstallerController<>::Events::BlockOnInstaller{});
            if (!ok) {
                return E_FAIL;
            }
            HRESULT hr = E_NOTIMPL;
            std::visit(internal::overloaded{
                         [&](InstallerController<>::States::Success& s) { hr = S_OK; },
                         [&](InstallerController<>::States::UpstreamDefaultInstall& s) { hr = s.hr; },
                         [&](auto&&... s) { hr = E_UNEXPECTED; },
                       },
                       ok.value());
            return hr;
        }
    } // namespace.

// The part of the code affected by the existence of the splash application in the package is excluded from compilation
// for ARM64 due splash application being written in Dart/Flutter, which currently does not supported Windows ARM64
// targets. See: https://github.com/flutter/flutter/issues/62597
#ifndef _M_ARM64
    namespace
    {
        std::filesystem::path splashPath()
        {
            return Win32Utils::thisAppRootdir() / L"ubuntu_wsl_splash.exe";
        }
    } // namespace.

    SplashEnabledStrategy::SplashEnabledStrategy() : splashExePath{splashPath()}
    {  }

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

    HRESULT SplashEnabledStrategy::do_install(Mode ui_mode)
    {
        std::array<InstallerController<>::Event, 3> eventSequence{
          InstallerController<>::Events::InteractiveInstall{ui_mode}, InstallerController<>::Events::StartInstaller{},
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
        return Oobe::do_reconfigure(installer);
    }

    HRESULT SplashEnabledStrategy::do_autoinstall(const std::filesystem::path& autoinstall_file)
    {
        return Oobe::do_autoinstall(installer, autoinstall_file);
    }

#else // _M_ARM64

    HRESULT NoSplashStrategy::do_install(Mode ui)
    {
        std::array<InstallerController<>::Event, 3> eventSequence{InstallerController<>::Events::InteractiveInstall{ui},
                                                                  InstallerController<>::Events::StartInstaller{},
                                                                  InstallerController<>::Events::BlockOnInstaller{}};
        HRESULT hr = E_NOTIMPL;
        for (auto& ev : eventSequence) {
            auto ok = installer.sm.addEvent(ev);

            // unexpected transition occurred here?
            if (!ok.has_value()) {
                return hr;
            }

            std::visit(internal::overloaded{
                         [&](InstallerController<>::States::Success& s) { hr = S_OK; },
                         [&](InstallerController<>::States::UpstreamDefaultInstall& s) { hr = s.hr; },
                         [&](auto&&... s) { hr = E_UNEXPECTED; },
                       },
                       ok.value());
        }

        return hr;
    }

    HRESULT NoSplashStrategy::do_reconfigure()
    {
        return Oobe::do_reconfigure(installer);
    }

    HRESULT NoSplashStrategy::do_autoinstall(std::filesystem::path autoinstall_file)
    {
        return Oobe::do_autoinstall(installer, autoinstall_file);
    }

#endif // _M_ARM64

} // namespace Oobe.
