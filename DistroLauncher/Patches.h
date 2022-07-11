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
    const std::wstring linux_dir = L"/var/lib/WSL/";
    const auto install_log = linux_dir + L"patches.log";                           // What patches are installed
    const auto output_log = linux_dir + L"patches_output.log";                     // Last patch install run
    constexpr std::wstring_view windows_dir = L"C:/Users/edu19/Work/WSL/patches/"; // Location of patches
}

struct PatchLog
{
    PatchLog(std::wstring_view linuxpath);

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
