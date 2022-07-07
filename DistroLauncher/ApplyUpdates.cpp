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

class VersionChanges
{
  public:
    virtual bool check_and_apply(PACKAGE_VERSION) const = 0;
    virtual PACKAGE_VERSION version() const noexcept = 0;
};

class VersionChanges2210_0_88_0 : public VersionChanges
{
    // Replace with std::wstring_view::starts_with in C++20
    [[nodiscard]] static bool starts_with(const std::wstring_view str, const std::wstring_view pattern)
    {
        return (str.size() >= pattern.size()) &&
               (std::mismatch(pattern.cbegin(), pattern.cend(), str.cbegin()).first == pattern.cend());
    }

    // Replace with std::wstring_view::ends_with in C++20
    [[nodiscard]] static bool ends_with(const std::wstring_view str, const std::wstring_view pattern)
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

    [[nodiscard]] static std::wstring_view GetDefaultUpgradePolicy()
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

    static HRESULT CreateVersionFileFolder(DWORD& exitCode)
    {
        const std::wstring linux_path = L"/var/lib/wsl/";
        if (std::filesystem::exists(Oobe::WindowsPath(linux_path))) {
            return S_OK;
        }
        std::wstring command = L"mkdir " + linux_path;
        return WslLaunchInteractiveAsRoot(command.c_str(), 1, &exitCode);
    }

    static HRESULT OverrideReleaseUpdatePolicy(DWORD& exitCode)
    {
        std::wstringstream command{};
        command << LR"(sed -i "s/^Prompt\w*[=:].*$/Prompt=)" << GetDefaultUpgradePolicy()
                << LR"(/" "/etc/update-manager/release-upgrades")";

        if constexpr (!is_debug()) {
            command << L"&> /dev/null";
        }

        return WslLaunchInteractiveAsRoot(command.str().c_str(), 1, &exitCode);
    }

  public:
    PACKAGE_VERSION version() const noexcept override
    {
        return Version::make(2210, 0, 88, 0);
    }

    bool check_and_apply(PACKAGE_VERSION curr_version) const override
    {
        if (Version::left_is_newer(curr_version, version())) {
            return true;
        }

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
    VersionFile version_file{L"/var/lib/wsl/launcher.version"};
    PACKAGE_VERSION version = version_file.read();

    std::array<std::unique_ptr<VersionChanges>, 1> change_set = {
      std::make_unique<VersionChanges2210_0_88_0>()
      // ...
      // future upgrades here
    };

    for (auto& version_changes : change_set) {
        const bool success = version_changes->check_and_apply(version);
        if (!success) {
            break;
        }
        version = std::max(version, version_changes->version(), Version::left_is_older);
    }
    version_file.write(version);
}
