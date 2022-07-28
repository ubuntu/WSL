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

#include "state_machine.h"
#include "installer_policy.h"
#include "installer_controller.h"

namespace Oobe
{
    using Mode = InstallerController<>::Mode;
    // This strategy fulfills the essential API required to run the OOBE without the complexity required by the
    // synchronization of the slide show. That separation is necessary because Flutter is not supported on Windows
    // ARM64. See: https://github.com/flutter/flutter/issues/62597
    struct NoSplashStrategy
    {
        InstallerController<> installer;

        /// Prints to the console an error message informing that this platform doesn't support running the splash
        /// screen.
        static void do_run_splash(bool hideConsole = false);

        /// Places the sequence of events to make the OOBE perform an automatic installation.
        HRESULT do_autoinstall(const std::filesystem::path& autoinstall_file);

        /// Places the sequence of events to make the OOBE perform an interactive installation.
        HRESULT do_install(Mode uiMode);

        /// Places the sequence of events to make the OOBE perform an automatic installation.
        HRESULT do_reconfigure();
    };
}
