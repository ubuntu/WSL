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
    std::wstring& trim(std::wstring& str)
    {
        constexpr std::wstring_view ws = L"\n\t ";
        const auto not_whitespace = [&](auto ch) { return std::find(ws.cbegin(), ws.cend(), ch) == ws.cend(); };
        str.erase(str.cbegin(), std::find_if(str.cbegin(), str.cend(), not_whitespace));
        str.erase(std::find_if(str.crbegin(), str.crend(), not_whitespace).base(), str.cend());
        return str;
    }
}

PatchLog::PatchLog(std::wstring_view linuxpath) : linux_path(linuxpath), windows_path(Oobe::WindowsPath(linuxpath))
{ }

bool PatchLog::exists() const
{
    return std::filesystem::exists(windows_path);
}

void PatchLog::read()
{
    patches = std::vector<std::wstring>{};
    if (!exists()) {
        return;
    }

    std::wifstream f(windows_path);
    std::wstring buffer;
    while (f) {
        std::getline(f, buffer);

        trim(buffer);

        // Comments and empty lines
        if (buffer.empty() || buffer.front() == '#') {
            continue;
        }

        patches.push_back(std::move(buffer));
        buffer.clear();
    }
}

void PatchLog::write()
{
    DWORD exitCode;
    std::wstringstream command;
    command << L"echo \"# WSL patches log. Do not modify this file.\n";
    std::for_each(patches.cbegin(), patches.cend(), [&](auto patch) { command << patch << '\n'; });
    command << "\" > " << linux_path;

    g_wslApi.WslLaunchInteractive(command.str().c_str(), 1, &exitCode);
}

void PatchLog::push_back(std::wstring patchname)
{
    patches.emplace_back(std::move(patchname));
};

bool PatchLog::contains(std::wstring_view patchname) const
{
    return std::find(patches.cbegin(), patches.cend(), patchname) != patches.cend();
}

// Imports all patches in [cbegin, cend), and returns an iterator past the last patch to be succesfully imported
std::vector<std::wstring>::const_iterator ImportPatches(std::vector<std::wstring>::const_iterator cbegin,
                                                        std::vector<std::wstring>::const_iterator cend)
{
    const std::wstring shutdownCmd = L"wsl -t " + DistributionInfo::Name;
    if (int cmdResult = _wsystem(shutdownCmd.c_str()); cmdResult != 0) {
        return cbegin;
    }

    return std::find_if_not(cbegin, cend, [](auto patchname) {
        const auto patch_windows_path = (std::filesystem::path{patches::windows_dir} += patchname) += L".diff";
        std::error_code errcode;
        bool success = std::filesystem::copy_file(patch_windows_path, Oobe::WindowsPath(patches::tmp_diff_path),
                                                  std::filesystem::copy_options::overwrite_existing, errcode);
        return success && !errcode;
    });
}

bool ApplyPatch(std::wstring_view patchname)
{
    DWORD errorCode;
    std::wstringstream command;

    command << L"patch -ruN < \"" << patches::tmp_diff_path << "\" &> \"" << patches::output_log << '"';
    const HRESULT hr = g_wslApi.WslLaunchInteractive(command.str().c_str(), 0, &errorCode);

    return SUCCEEDED(hr) && errorCode == 0;
}

[[nodiscard]] std::vector<std::wstring> PatchList()
{
    return {L"2210_0_8_0-0001"};
}

void ApplyPatches()
{
    PatchLog patch_log{patches::install_log};

    if (!patch_log.exists()) {
        DWORD exitCode;
        HRESULT hr = WslLaunchInteractiveAsRoot((L"mkdir -p " + patches::linux_dir).c_str(), 1, &exitCode);
        if (FAILED(hr) || exitCode != 0) {
            return;
        }
    }

    patch_log.read();
    const auto patchlist = PatchList();

    // Filter patches already applied
    const auto patches_begin =
      std::find_if_not(patchlist.cbegin(), patchlist.cend(), [&](auto pname) { return patch_log.contains(pname); });

    if (patches_begin == patchlist.cend()) {
        return;
    }

    WslAsRoot([&]() {
        // Import patches
        auto patches_end = ImportPatches(patches_begin, patchlist.cend());

        // Apply successfully imported, non-redundant patches
        patches_end = std::find_if_not(patches_begin, patches_end, ApplyPatch);

        // Log applied patches
        std::for_each(patches_begin, patches_end, [&](auto& pname) { patch_log.push_back(std::move(pname)); });
        patch_log.write();
    });
}
