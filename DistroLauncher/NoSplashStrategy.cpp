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
#include "NoSplashStrategy.h"

namespace Oobe
{
    void NoSplashStrategy::do_run_splash(bool hideConsole)
    {
        wprintf(L"This device architecture doesn't support running the splash screen.\n");
    }

    HRESULT NoSplashStrategy::do_install(Mode uiMode)
    {
        // Notice that both GUI and TUI are supported. This strategy doesn't offer the slide show only.
        return internal::install_linux_ui(installer, uiMode);
    }

    HRESULT NoSplashStrategy::do_reconfigure()
    {
        return internal::reconfigure_linux_ui(installer);
    }

    HRESULT NoSplashStrategy::do_autoinstall(const std::filesystem::path& autoinstall_file)
    {
        return internal::do_autoinstall(installer, autoinstall_file);
    }
}
