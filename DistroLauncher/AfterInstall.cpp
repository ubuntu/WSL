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

// Replace with std::wstring_view::starts_with in C++20
[[nodiscard]] bool starts_with(const std::wstring_view str, const std::wstring_view pattern)
{
    return (str.size() >= pattern.size()) &&
           (std::mismatch(pattern.cbegin(), pattern.cend(), str.cbegin()).first == pattern.cend());
}

// Replace with std::wstring_view::ends_with in C++20
[[nodiscard]] bool ends_with(const std::wstring_view str, const std::wstring_view pattern)
{
    return (str.size() >= pattern.size()) &&
           (std::mismatch(pattern.crbegin(), pattern.crend(), str.crbegin()).first == pattern.crend());
}

[[nodiscard]] std::wstring_view GetDefaultUpgradePolicy()
{
    const std::wstring_view app_id = DistributionInfo::Name;
    if (app_id == L"Ubuntu (Preview)")
        return L"normal";
    if (app_id == L"Ubuntu")
        return L"lts";
    if (starts_with(app_id, L"Ubuntu") && ends_with(app_id, L"LTS"))
        return L"never";
    return L"normal"; // Default to development build
}

void OverrideUpgradePolicy()
{
    std::wstringstream ss_command{};
    ss_command << LR"(sed -i "s/^Prompt\w*[=:].*$/Prompt=)" << GetDefaultUpgradePolicy()
               << LR"(/" "/etc/update-manager/release-upgrades")";
    const auto command = ss_command.str();
    DWORD exitCode;
    g_wslApi.WslLaunchInteractive(command.c_str(), 1, &exitCode);
}

void AfterInstall()
{
    OverrideUpgradePolicy();
}