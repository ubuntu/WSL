#pragma once
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

#include <mutex>
#include "state_machine.h"
#include "installer_policy.h"
#include "installer_controller.h"
#include "local_named_pipe.h"
#include "console_service.h"
#include "splash_controller.h"

namespace Oobe
{
    using Mode = InstallerController<>::Mode;
    // This strategy fulfills the essential API required by the Oobe::Application<> class and augment it with the
    // machinery necessary to coordinate the splash screen application, the console service for console redirection and
    // visibility toggling and the OOBE itself running inside Ubuntu.
    class SplashEnabledStrategy
    {
      private:
        // This mutex must be acquired before calling ConsoleService::redirectConsole() and
        // ConsoleService::restoreConsole(). That's a consequence of using RegisterWaitSingleObject to notify if the
        // user closes the splash application. That function imposes concurrent access to this class and could cause
        // races and deadlocks if all stars and planets aligned.
        std::timed_mutex consoleGuard;
        bool consoleIsVisible = true;
        bool splashIsRunning = false;
        std::filesystem::path splashExePath;

        HANDLE consoleReadHandle = nullptr;
        InstallerController<> installer;
        // optionals for lazy creation.
        std::optional<SplashController<>> splash;
        std::optional<HWND> splashWindow;
        std::optional<Win32Utils::ConsoleService<Win32Utils::LocalNamedPipe>> console;

        /// Restores the console context, which means undo the console redirection and making the window visible.
        void do_show_console();

        /// Hides the splash window.
        void do_toggle_splash();

        /// Notifies the splash controller to quit the application. Console restoration is immediately requested to
        /// prevent leaving the launcher process without a stdout consumer.
        void do_close_splash();

      public:
        /// Places the sequence of events to make the OOBE perform an automatic installation.
        HRESULT do_autoinstall(const std::filesystem::path& autoinstall_file);

        /// Places the sequence of events to make the OOBE perform an interactive installation.
        /// By default GUI support is checked before launching the OOBE, unless [forceTextMode]
        /// is set to true, in which case the check is skipped and the installer run in TUI mode.
        HRESULT do_install(Mode uiMode);

        /// Places the sequence of events to make the OOBE perform an automatic installation.
        HRESULT do_reconfigure();

        /// Triggers the console redirection and launch the splash screen application.
        void do_run_splash(bool hideConsole = false);

        SplashEnabledStrategy();
        ~SplashEnabledStrategy() = default;
    };
}
