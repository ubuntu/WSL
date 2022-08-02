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

namespace internal
{

    std::wstring GetUpgradePolicy()
    {
        std::wstring_view name = DistributionInfo::Name;
        if (name == L"Ubuntu") {
            return L"lts";
        }
        if (starts_with(name, L"Ubuntu") && ends_with(name, L"LTS")) {
            return L"never";
        }
        return L"normal";
    }

    void SetDefaultUpgradePolicyImpl()
    {
        namespace fs = std::filesystem;

        const fs::path log{L"/var/log/upgrade-policy-changed.log"};
        const fs::path policyfile{L"/etc/update-manager/release-upgrades"};

        if (Oobe::WslFileExists(log)) {
            return;
        }

        std::wstring regex = concat(L"s/Prompt=lts/Prompt=", GetUpgradePolicy(), L'/');
        std::wstring sed = concat(L"sed -i ", std::quoted(regex), L' ', policyfile);
        std::wstring date = concat(L"date --iso-8601=seconds > ", log);

        std::wstring command = concat(L"bash -ec ", std::quoted(concat(sed, L" && ", date)));

        DWORD errCode;
        Sudo::WslLaunchInteractive(command.c_str(), FALSE, &errCode);
    }

}

void SetDefaultUpgradePolicy()
{
    auto mutex = NamedMutex(L"upgrade-policy");
    mutex.lock().and_then(internal::SetDefaultUpgradePolicyImpl);
}
