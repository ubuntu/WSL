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
#include "snapd.h"

namespace Oobe::internal
{
    std::wstring TempDisableSnapdImpl(WslApiLoader& api, const std::wstring& distroName)
    {
        // Disables snapd.service and patches wsl-setup to find the snaps inside the seed directory
        // since most likely snap will have messed with the hard links.
        // This fails (exit 1) if the snapd version doesn't match the bad one or the subsequent commands fail.
        const wchar_t* disableSnapd_v2_57_4_script =
          LR"( [[ "$(dpkg-query --show snapd | cut -f2) " == "2.57.4"* ]] && ln -sT /dev/null /etc/systemd/system/snapd.service && sed -i 's#/var/lib/snapd/snaps/#/var/lib/snapd/seed/snaps/#g' /usr/libexec/wsl-setup )";
        const std::wstring script = concat(L"bash -ec ", std::quoted(disableSnapd_v2_57_4_script));

        DWORD exitCode = 0;
        api.WslLaunchInteractive(script.c_str(), FALSE, &exitCode);

        if (exitCode == 0) {
            const std::wstring cmd = concat(L"wsl -t ", distroName);
            _wsystem(cmd.c_str());
        }

        return L"rm /etc/systemd/system/snapd.service || true";
    }

}
