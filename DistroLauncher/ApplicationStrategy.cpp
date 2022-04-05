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
            HRESULT hr = E_NOTIMPL;

            auto ok = controller.sm.addEvent(InstallerController<>::Events::Reconfig{});
            if (!ok) {
                return hr;
            }

            std::visit(internal::overloaded{
                         [&](InstallerController<>::States::Success& s) { hr = S_OK; },
                         [&](InstallerController<>::States::UpstreamDefaultInstall& s) { hr = s.hr; },
                         [&](auto&&... s) { hr = E_UNEXPECTED; },
                       },
                       ok.value());
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
            const wchar_t* splashName = L"ubuntu_wsl_splash.exe";
            TCHAR launcherName[MAX_PATH];
            DWORD fnLength = GetModuleFileName(nullptr, launcherName, MAX_PATH);
            if (fnLength == 0) {
                return splashName;
            }
            std::filesystem::path splashPath{std::wstring_view{launcherName, fnLength}};
            splashPath.replace_filename(splashName);

            return splashPath;
        }
    } // namespace.

    SplashEnabledStrategy::SplashEnabledStrategy() : splashExePath{splashPath()}
    { }

    void SplashEnabledStrategy::do_run_splash()
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

        console.value().hideConsoleWindow();
        consoleIsVisible = false;
        splashIsRunning = true;
    }

    void SplashEnabledStrategy::do_toggle_splash()
    {
        splash.value().sm.addEvent(SplashController<>::Events::ToggleVisibility{});
    }

    void SplashEnabledStrategy::do_show_console()
    {
        if (consoleIsVisible) {
            return;
        }
        std::unique_lock<std::timed_mutex> guard{consoleGuard, std::defer_lock};
        using namespace std::chrono_literals;
        constexpr auto tryLockTimeout = 5s;
        if (!guard.try_lock_for(tryLockTimeout)) {
            wprintf(L"Failed to lock console state for modification. Somebody else is holding the lock.\n");
            return;
        }
        console.value().showConsoleWindow();
        console.value().restoreConsole();
        consoleIsVisible = true;
    }

    void SplashEnabledStrategy::do_close_splash()
    {
        if (splashIsRunning) {
            splash.value().sm.addEvent(SplashController<>::Events::Close{&(splash.value())});
            splashIsRunning = false;
        }
        do_show_console();
    }

    HRESULT SplashEnabledStrategy::do_install()
    {
        std::array<InstallerController<>::Event, 3> eventSequence{InstallerController<>::Events::InteractiveInstall{},
                                                                  InstallerController<>::Events::StartInstaller{},
                                                                  InstallerController<>::Events::BlockOnInstaller{}};
        HRESULT hr = E_NOTIMPL;
        for (auto& ev : eventSequence) {
            auto ok = installer.sm.addEvent(ev);

            // unexpected transition occurred here?
            if (!ok.has_value()) {
                do_close_splash();
                return E_FAIL;
            }

            std::visit(internal::overloaded{
                         [&](InstallerController<>::States::PreparedTui& s) { do_show_console(); },
                         [&](InstallerController<>::States::Ready& s) { do_toggle_splash(); },
                         [&](InstallerController<>::States::Success& s) {
                             do_close_splash();
                             hr = S_OK;
                         },
                         [&](InstallerController<>::States::UpstreamDefaultInstall& s) {
                             do_show_console();
                             hr = s.hr;
                         },
                         [&](auto&&... s) { hr = E_UNEXPECTED; },
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

    HRESULT NoSplashStrategy::do_install()
    {
        std::array<InstallerController<>::Event, 3> eventSequence{InstallerController<>::Events::InteractiveInstall{},
                                                                  InstallerController<>::Events::StartInstaller{},
                                                                  InstallerController<>::Events::BlockOnInstaller{}};
        HRESULT hr = E_NOTIMPL;
        for (auto& ev : eventSequence) {
            auto ok = installer.sm.addEvent(ev);

            // unexpected transition occurred here?
            if (!ok.has_value()) {
                return E_FAIL;
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
