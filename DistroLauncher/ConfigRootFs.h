#pragma once
/*
 * Copyright (C) 2023 Canonical Ltd
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

namespace Ubuntu
{
    /// Performs post-registration file system configuration and tweaks to optimize the behavior of the distro image to WSL.
    /// The actions come in two distinct natures: config file patching and systemd unit disabling.
    /// See [ApplyConfigPatches] and [DisableSystemdUnits] for details.
    void ConfigRootFs(const std::wstring& DistroName, WslApiLoader& wsl);
    /**
     * The distro launcher is equipped with a simple mechanism to adapt the root filesystem contents to some WSL
     * specificities through patching distro configuration files.
     * Those patches are idempotent, small and atomic, in the sense that each patching action either completes
     * successfully or does nothing to the system state. Patches are implemented as C++ functions. See
     * [PatchConfig.h/cpp] for the implementation details.
     *
     * Those patches must only be applied right after registration, where:
     * 1. Root filesystem access is guaranteed.
     * 2. We can assume all existing patches are missing. No tracking of which patches were applied and when.
     * 3. No decision making is necessary on to whether this is the right time to try patching the config files or not.
     * 4. No pessimisation of regular boot time.
     *
     * Respecting the assumptions above allow for very simple patching functions.
     *
     * Patches may exist in two forms:
     * 1. Release agnostic patches. Are required for all Ubuntu apps.
     * 2. Release specific patches. Only the specific [DistroName] requires it.
     *
     */

    /// Applies any relevant patch for the [DistroName].
    void ApplyConfigPatches(std::wstring_view DistroName);

    /// Disables the relevant systemd units by means of running `systemctl disable`.
    void DisableSystemdUnits(WslApiLoader& wsl);
}
