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

#include "stdafx.h"
#include "Patch.h"
#include "ConfigRootfs.h"

namespace Ubuntu
{
    void ConfigRootFs(const std::wstring& DistroName, WslApiLoader& wsl)
    {
        ApplyConfigPatches(DistroName);
        // Currently the only systemd unit mapped for explicit disablement is ssh on 20.04.
        // If other cases appear, we should follow some strategy similar to the ApplyConfigPatches
        // to properly select which units to disable for which distro release. The selection should still
        // be internal, i.e. non visible from DistroLauncher.cpp.
        if (DistroName == L"Ubuntu-20.04") {
            DisableSystemdUnits(wsl);
        }
    }

    void ApplyConfigPatches(std::wstring_view DistroName)
    {
        // I think the WslPathPrefix no longer belongs to Oobe namespace. But let's leave that for later.
        std::filesystem::path pathPrefix{Oobe::WslPathPrefix()};
        pathPrefix /= DistroName;

        // Apply global patches
        for (const auto& patch : releaseAgnosticPatches) {
            patch.apply(pathPrefix);
        }

        // check for release specific patches.
        const auto& releaseSpecific = releaseSpecificPatches.find(DistroName);
        if (releaseSpecific == releaseSpecificPatches.end()) {
            return;
        }

        // apply them if any.
        for (const auto& patch : releaseSpecific->second) {
            patch.apply(pathPrefix);
        }
    }

    void DisableSystemdUnits(WslApiLoader& wsl)
    {
        DWORD exitCode;
        auto hr = wsl.WslLaunchInteractive(L"systemctl disable ssh &>/dev/null", true, &exitCode);
    }
}
