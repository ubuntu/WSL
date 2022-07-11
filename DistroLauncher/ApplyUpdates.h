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

constexpr std::wstring_view patches_log_linux_path = L"/var/lib/WSL/patches.log";
constexpr std::wstring_view patches_windows_path = L"patches/";

struct PatchLog
{
    PatchLog(std::wstring_view linuxpath);

    std::filesystem::path linux_path;
    std::filesystem::path windows_path;

    [[nodiscard]] bool exists() const;

    void read();
    void write();

    void push_back(std::wstring_view patchname);
    [[nodiscard]] bool contains(std::wstring_view patchname) const;

  private:
    std::vector<std::wstring> data;
};

void ApplyPatches();
