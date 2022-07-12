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
#include <iomanip>

std::wstring& trim(std::wstring& str)
{
    constexpr std::wstring_view ws = L"\n\t ";
    const auto not_whitespace = [&](auto ch) { return std::find(ws.cbegin(), ws.cend(), ch) == ws.cend(); };
    str.erase(str.cbegin(), std::find_if(str.cbegin(), str.cend(), not_whitespace));
    str.erase(std::find_if(str.crbegin(), str.crend(), not_whitespace).base(), str.cend());
    return str;
}

PatchLog::PatchLog(std::wstring_view linuxpath) : linux_path(linuxpath), windows_path(Oobe::WindowsPath(linuxpath))
{ }

PatchLog::PatchLog(std::filesystem::path linuxpath) :
    linux_path(std::move(linuxpath)), windows_path(Oobe::WindowsPath(linux_path))
{ }

bool PatchLog::exists() const
{
    return std::filesystem::exists(windows_path);
}

void PatchLog::read()
{
    patches = std::vector<std::wstring>{};
    if (!exists()) {
        any_changes = true;
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
    if (!any_changes) {
        return;
    }
    DWORD exitCode;
    std::wstringstream command;
    command << L"echo \"# WSL patches log. Do not modify this file.\n";
    std::for_each(patches.cbegin(), patches.cend(), [&](auto patch) { command << patch << '\n'; });
    command << "\" > " << linux_path;

    g_wslApi.WslLaunchInteractive(command.str().c_str(), 1, &exitCode);
}

void PatchLog::push_back(std::wstring patchname)
{
    any_changes = true;
    patches.emplace_back(std::move(patchname));
};

bool PatchLog::contains(std::wstring_view patchname) const
{
    return std::find(patches.cbegin(), patches.cend(), patchname) != patches.cend();
}

bool CreateLogDirectory()
{
    DWORD exitCode;
    const auto command = L"mkdir -p " + patches::linux_dir.wstring();
    HRESULT hr = WslLaunchInteractiveAsRoot(command.c_str(), 1, &exitCode);
    return SUCCEEDED(hr) && exitCode == 0;
}

bool ShutdownDistro()
{
    const std::wstring shutdownCmd = L"wsl -t " + DistributionInfo::Name;
    if (int cmdResult = _wsystem(shutdownCmd.c_str()); cmdResult != 0) {
        return false;
    }
    return true;
}

bool ImportPatch(std::wstring_view patchname)
{
    const auto patch_windows_path{(std::filesystem::path{patches::windows_dir} += patchname) += L".diff"};
    const auto patch_wsl_tmp_path{Oobe::WindowsPath(patches::tmp_location)};

    std::error_code errcode;
    bool success = std::filesystem::copy_file(patch_windows_path, patch_wsl_tmp_path,
                                              std::filesystem::copy_options::overwrite_existing, errcode);
    return success && !errcode;
}

bool ApplyPatch(std::wstring_view patchname)
{
    DWORD errorCode;
    std::wstringstream command;

    const auto patch_linux_path = patches::tmp_location.wstring();
    const auto output_log_linux_path = patches::output_log.wstring();

    command << L"patch -ruN < " << std::quoted(patch_linux_path) << L" &>> " << std::quoted(output_log_linux_path);
    std::wcout << command.str() << std::endl;
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
        if (const bool success = CreateLogDirectory(); !success) {
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
        // Restart distro
        ShutdownDistro();

        // Import and apply patches
        auto patches_end = std::find_if_not(patches_begin, patchlist.cend(), [](auto patchname) {
            return ImportPatch(patchname) && ApplyPatch(patchname);
        });

        // Log applied patches
        std::for_each(patches_begin, patches_end, [&](auto& pname) { patch_log.push_back(std::move(pname)); });
        patch_log.write();
    });
}
