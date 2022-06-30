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
#include "AfterInstall.h"

// Replace with std::wstring_view::starts_with in C++20
[[nodiscard]] bool starts_with(const std::wstring_view str, const std::wstring_view other)
{
    return (str.size() >= other.size()) &&
           (std::mismatch(other.cbegin(), other.cend(), str.cbegin()).first == other.cend());
}

// Replace with std::wstring_view::ends_with in C++20
[[nodiscard]] bool ends_with(const std::wstring_view str, const std::wstring_view other)
{
    return (str.size() >= other.size()) &&
           (std::mismatch(other.crbegin(), other.crend(), str.crbegin()).first == other.crend());
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

HRESULT OverrideUpgradePolicy(DWORD& exitCode)
{
    std::wstringstream ss_command{};
    ss_command << LR"(sed -i "s/^Prompt\w*[=:].*$/Prompt=)" << GetDefaultUpgradePolicy()
               << LR"(/" "/etc/update-manager/release-upgrades")";
    const auto command = ss_command.str();
    return g_wslApi.WslLaunchInteractive(command.c_str(), true, &exitCode);
}

HRESULT AfterInstall(DWORD& exitCode)
{
    HRESULT hr;
    if (hr = OverrideUpgradePolicy(exitCode); FAILED(hr)) {
        return hr;
    }

    return hr;
}