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

/// Functions that feed application strategies with common behavior in which the setup UI runs inside the distro. No
/// Windows executables are involved. Those would be duplicated code if they were methods in specific strategy classes.
namespace Oobe::internal
{
    // Given an [installer] controller, places the sequence of events to execute an autoinstallation. Requires an
    // existing [autoinstall_file] to seed Subiquity with the required information.
    // That is a simple workflow that only depends on Subiquity.
    HRESULT do_autoinstall(InstallerController<>& controller, const std::filesystem::path& autoinstall_file);

    // Given an [installer], places the sequence of events to execute the setup in the required [ui] Mode.
    // Either GUI or TUI will run on Linux, thus WSLg is required for GUI.
    // This is meant for situations which don't require interlocking with other processes, such as a splash window.
    HRESULT install_linux_ui(Oobe::InstallerController<>& installer, Mode uiMode);

    // Given an [installer], places the sequence of events to execute the reconfiguration workflow.
    // The UI mode is autodetected. Either GUI or TUI will run on Linux, thus WSLg is required for GUI.
    // This is meant for situations which don't require interlocking with other processes, such as a splash window.
    HRESULT reconfigure_linux_ui(Oobe::InstallerController<>& installer);
}
