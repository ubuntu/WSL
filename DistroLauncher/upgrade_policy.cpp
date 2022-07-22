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

bool starts_with(std::wstring_view tested, std::wstring_view start)
{
    if (tested.size() < start.size()) {
        return false;
    }
    auto mismatch = std::mismatch(start.cbegin(), start.cend(), tested.cbegin());
    return mismatch.first == start.cend();
}

bool ends_with(std::wstring_view tested, std::wstring_view end)
{
    if (tested.size() < end.size()) {
        return false;
    }
    auto mismatch = std::mismatch(end.crbegin(), end.crend(), tested.crbegin());
    return mismatch.first == end.crend();
}

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

    if (fs::exists(log)) {
        return;
    }

    std::wstring regex = L"s/Prompt=lts/Prompt=" + GetUpgradePolicy() + L'/';
    std::wstringstream sed;
    sed << L"sed -i " << std::quoted(regex) << L' ' << std::quoted(policyfile.wstring());

    DWORD errCode;
    auto hr = Sudo::WslLaunchInteractive(sed.str().c_str(), FALSE, &errCode);

    if (SUCCEEDED(hr) && errCode == 0) {
        std::wstringstream cmd;
        cmd << L"date --iso-8601=seconds" << L" > " << std::quoted(log.wstring());
        Sudo::WslLaunchInteractive(cmd.str().c_str(), FALSE, &errCode);
    }
}

void SetDefaultUpgradePolicy()
{
    auto mutex = NamedMutex(L"upgrade-policy");
    mutex.lock().and_then(SetDefaultUpgradePolicyImpl);
}
