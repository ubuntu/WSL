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
#include "installer_policy.h"
#include "installer_controller.h"

namespace Oobe
{
    using Mode = InstallerController<>::Mode;
    /// Concrete strategy implementation to fulfill the [Application] class for Windows platforms that don't support
    /// running the Flutter OOBE. In such platforms only TUI and the upstream default experiences are offered because
    /// the UDI snap has been replaced by Subiquity-only snap.
    class WinTuiStrategy
    {
      public:
        /// Places the sequence of events to make Subiquity perform an automatic installation.
        /// Does not run the Flutter GUI (not even for the slide show).
        HRESULT do_autoinstall(const std::filesystem::path& autoinstall_file);

        /// Performs an interactive installation.
        /// By default GUI support is checked before launching the OOBE.
        HRESULT do_install(Mode uiMode);

        /// Runs the reconfiguration variant of the Flutter GUI.
        HRESULT do_reconfigure();

        /// Triggers the console redirection and launch the OOBE slide show. Not supported by this strategy.
        static void do_run_splash(bool hideConsole = false);

        WinTuiStrategy() = default;
        ~WinTuiStrategy() = default;

      private:
        InstallerController<> installer;
    };

}
