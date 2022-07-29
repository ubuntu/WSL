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
#include "WinTuiStrategy.h"

namespace Oobe
{
    HRESULT WinTuiStrategy::do_autoinstall(const std::filesystem::path& autoinstall_file)
    {
        return internal::do_autoinstall(installer, autoinstall_file);
    }

    HRESULT WinTuiStrategy::do_install(Mode uiMode)
    {
        if (uiMode == Mode::Gui) {
            wprintf(L"GUI mode is not supported on this platform.\n");
        }
        return internal::install_linux_ui(installer, Mode::Text);
    }

    HRESULT WinTuiStrategy::do_reconfigure()
    {
        return internal::reconfigure_linux_ui(installer);
    }

    void WinTuiStrategy::do_run_splash(bool hideConsole)
    {
        wprintf(L"This device architecture doesn't support running the splash screen.\n");
    }
}
