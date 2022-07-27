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

namespace Oobe
{
    /// Decouples parsing the environment variable from the rest of the GUI support detection
    /// routine for the OOBE.
    enum class LauncherForceMode : wchar_t
    {
        Invalid,
        Unset = L'0',
        TextForced = L'1',
        GuiForced = L'2',
        min = Unset,
        max = GuiForced,
    };

    /// Returns a [LauncherForceMode] value by reading the LAUNCHER_FORCE_MODE environment variable,
    /// which can only be:
    ///	0 or unset or invalid = autodetection
    ///	1 = text mode
    ///	2 = GUI mode.
    LauncherForceMode environmentForceMode();

}
