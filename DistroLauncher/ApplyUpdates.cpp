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
    static constexpr auto versionfile_linux_path = L"/var/lib/wsl/launcher.version";
}

namespace change_2210_0_88_1
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

    static constexpr bool is_debug()
    {
#ifdef DNDEBUG
        return false;
#else
        return true;
#endif
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

    HRESULT CreateVersionFileFolder(DWORD& exitCode)
    {
        const std::wstring linux_path = std::filesystem::path{versionfile_linux_path}.parent_path().wstring();
        if (std::filesystem::exists(Oobe::WindowsPath(linux_path))) {
            exitCode = 0;
            return S_OK;
        }
        std::wstring command = L"mkdir " + linux_path;
        return WslLaunchInteractiveAsRoot(command.c_str(), 1, &exitCode);
    }

    HRESULT OverrideReleaseUpdatePolicy(DWORD& exitCode)
    {
        std::wstringstream command{};
        command << LR"(sed -i "s/^Prompt\w*[=:].*$/Prompt=)" << GetDefaultUpgradePolicy()
                << LR"(/" "/etc/update-manager/release-upgrades")";

        if constexpr (!is_debug()) {
            command << L"&> /dev/null";
        }

        return WslLaunchInteractiveAsRoot(command.str().c_str(), 1, &exitCode);
    }

    const inline PACKAGE_VERSION version = Version::make(2210, 0, 88, 1);

    bool requires_update(PACKAGE_VERSION installed_version)
    {
        return Version::left_is_older(installed_version, version);
    }

    bool apply_changes()
    {
        // Creating launcher version parent directory
        {
            DWORD exitCode;
            HRESULT hr = CreateVersionFileFolder(exitCode);
            if (FAILED(hr) || exitCode != 0) {
                std::wcerr << L"Failed to start launcher version directory.";
                return false;
            }
        }

        // Overriding release update policy
        {
            DWORD exitCode;
            const HRESULT hr = OverrideReleaseUpdatePolicy(exitCode);
            if (FAILED(hr) || exitCode != 0) {
                if constexpr (is_debug()) {
                    std::wcerr << L"Failed to set up default release update policy.\n";
                }
                return false;
            }
        }
        return true;
    }
};

void ApplyUpdates()
{
    VersionFile version_file{versionfile_linux_path};
    PACKAGE_VERSION version = version_file.read();

    {
        namespace Change = change_2210_0_88_1;
        if (Change::requires_update(version)) {
            const bool success = Change::apply_changes();
            if (!success) {
                return;
            }
            version = Change::version;
            version_file.write(version);
        }
    }

    {
        // Future changes here
    }

    const PACKAGE_VERSION launcher_v = Version::current();
    if (Version::left_is_newer(launcher_v, version)) {
        version_file.write(launcher_v);
    }
}
