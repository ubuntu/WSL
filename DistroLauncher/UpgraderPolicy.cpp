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

namespace
{

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

    constexpr bool debug()
    {
#ifdef DNDEBUG
        return false;
#else
        return true;
#endif
    }

    HRESULT OverrideReleaseUpdatePolicyImpl(DWORD& exitCode)
    {
        std::wstringstream command{};
        command << LR"(sed -i "s/^Prompt\w*[=:].*$/Prompt=)" << GetDefaultUpgradePolicy()
                << LR"(/" "/etc/update-manager/release-upgrades")";

        if constexpr (!debug()) {
            command << L"&> /dev/null";
        }

        return WSLLaunchInteractiveAsRoot(command.str().c_str(), 1, &exitCode);
    }
}

void OverrideReleaseUpdatePolicy()
{
    // Checking if overriden already
    const auto file_linux = L"/etc/update-manager/wsl-launcher-do-not-override";
    const auto file_windows = Oobe::WindowsPath(file_linux);

    if (std::filesystem::exists(file_windows)) {
        return;
    }

    // Overriding
    DWORD exitCode;
    const HRESULT hr = OverrideReleaseUpdatePolicyImpl(exitCode);

    if (SUCCEEDED(hr) && !exitCode) {
        const auto command = std::wstring{L"touch "} + file_linux;
        WSLLaunchInteractiveAsRoot(command.c_str(), 1, &exitCode);
        return;
    }

    if constexpr (debug()) {
        std::wcerr << L"Failed to set up default release update policy.";
        if (SUCCEEDED(hr)) {
            std::wcerr << " Return code: " << exitCode;
        }
        std::wcerr << '\n';
    }
}
