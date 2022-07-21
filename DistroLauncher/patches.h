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
#pragma once

namespace patches
{
    using std::filesystem::path;

    const path log_dir{L"/var/log/wsl/"};
    const auto patch_log = path{log_dir} += L"patches.log";                // Log listing installed patches
    const auto patch_install_log = path{log_dir} += L"patches-output.log"; // stdout and stderr of patch install process
    const auto appx_patches_dir = Win32Utils::thisAppRootdir() / L"patches/"; // Location of patches in appx
    const path tmp_patch{L"/tmp/patch"}; // Location where patch is imported and applied from
}

struct PatchLog
{
    PatchLog(std::wstring_view linuxpath);
    PatchLog(std::filesystem::path linuxpath);

    std::filesystem::path linux_path;
    std::filesystem::path windows_path;

    [[nodiscard]] bool exists() const;

    void read();
    void write();

    void emplace_back(std::wstring&& patchname);

    [[nodiscard]] bool contains(std::wstring_view patchname) const;

  private:
    std::vector<std::wstring> patches;
    bool any_changes = false;
};

void ApplyPatches();
