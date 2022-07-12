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

#include "stdafx.h"

namespace patches
{
    const auto linux_dir = std::filesystem::path{L"/var/log"};
    const auto install_log = linux_dir / L"wsl-patches.log";              // What patches are installed
    const auto output_log = linux_dir / L"wsl-patches-output.log";        // Patch install stdout and stderr
    const auto windows_dir = Win32Utils::thisAppRootdir() /  L"patches/"; // Location of patches
    const auto tmp_location = std::filesystem::path{L"/tmp/patch.diff"};  // Location where patch is imported
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

    void push_back(std::wstring patchname);
    [[nodiscard]] bool contains(std::wstring_view patchname) const;

  private:
    std::vector<std::wstring> patches;
    bool any_changes = false;
};

void ApplyPatches();
