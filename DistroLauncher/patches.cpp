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

std::wstring& trim(std::wstring& str)
{
    constexpr std::wstring_view wspace = L"\n\t ";
    const auto not_whitespace = [&](wchar_t character) {
        return std::find(wspace.cbegin(), wspace.cend(), character) == wspace.cend();
    };
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

    std::wifstream file_handle(windows_path);
    std::wstring buffer;
    while (file_handle) {
        std::getline(file_handle, buffer);

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

void PatchLog::emplace_back(std::wstring&& patchname)
{
    any_changes = true;
    patches.emplace_back(std::forward<std::wstring>(patchname));
};

bool PatchLog::contains(std::wstring_view patchname) const
{
    return std::find(patches.cbegin(), patches.cend(), patchname) != patches.cend();
}

// (printf "\n[$(date --iso-8601=seconds)]: " && your command here ) >> $patch_install_log
template <typename... Args> std::wstring LoggedCommand(Args&&... args)
{
    const auto lx_output_log = patches::patch_install_log.wstring();
    std::wstringstream ss;
    ss << L"(printf \"\\n[$(date --iso-8601=seconds)]: \" && ";
    (ss << ... << std::forward<Args>(args));
    ss << L") >> " << std::quoted(lx_output_log) << L" 2>&1";
    return ss.str();
}
bool ShutdownDistro()
{
    const std::wstring shutdown_command = L"wsl -t " + DistributionInfo::Name;
    return _wsystem(shutdown_command.c_str()) == 0;
}

bool ImportPatch(std::wstring_view patchname)
{
    const auto wd_patch_path{std::filesystem::path{patches::appx_patches_dir} += patchname};
    const auto wd_patch_tmp_path{Oobe::WindowsPath(patches::tmp_patch)};

    std::error_code errCode;
    bool success = std::filesystem::copy_file(wd_patch_path, wd_patch_tmp_path,
                                              std::filesystem::copy_options::overwrite_existing, errCode);
    return success && !errCode;
}

bool ApplyPatch(std::wstring_view patchname)
{
    const auto lx_patch_path = patches::tmp_patch.wstring();

    // Setting as executable
    {
        std::wstring chmod = LoggedCommand(L"chmod +x ", std::quoted(lx_patch_path));
        DWORD exitCode = 1;
        HRESULT hr = g_wslApi.WslLaunchInteractive(chmod.c_str(), FALSE, &exitCode);
        if (FAILED(hr) || exitCode != 0) {
            return false;
        }
    }

    // Executing
    {
        std::wstring exec = LoggedCommand(lx_patch_path);
        DWORD exitCode = 1;
        HRESULT hr = g_wslApi.WslLaunchInteractive(exec.c_str(), FALSE, &exitCode);
        if (FAILED(hr) || exitCode != 0) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] std::optional<std::vector<std::wstring>> ReadPatchList()
{
    std::vector<std::wstring> patchnames;
    auto directory = std::filesystem::directory_iterator(patches::appx_patches_dir);

    for (const auto& patchfile : directory) {
        patchnames.push_back(patchfile.path().stem().wstring());
    }

    return patchnames;
}

void ApplyPatchesImpl()
{
    PatchLog patch_log{patches::patch_log};

    patch_log.read();
    auto patchlist = ReadPatchList();
    if (!patchlist.has_value()) {
        return;
    }

    // Filter patches already applied
    const auto patches_begin =
      std::find_if_not(patchlist->begin(), patchlist->end(), [&](auto pname) { return patch_log.contains(pname); });

    if (patches_begin == patchlist->cend()) {
        return;
    }

    Sudo()
      .and_then([&]() {
          // Restart distro
          ShutdownDistro();

          // Import and apply patches
          auto patches_end = std::find_if_not(patches_begin, patchlist->end(), [](auto patchname) {
              return ImportPatch(patchname) && ApplyPatch(patchname);
          });

          // Log applied patches
          std::for_each(patches_begin, patches_end, [&](auto& pname) { patch_log.emplace_back(std::move(pname)); });
          patch_log.write();
      })
      .or_else([](auto why) {
#ifndef DNDEBUG
          std::wcerr << "Failed to set root user during install. Error code: " << static_cast<int>(why) << std::endl;
#endif
      });
}

void ApplyPatches()
{
    static auto update_mutex = NamedMutex(L"install-mutex", true);
    update_mutex.lock().and_then(ApplyPatchesImpl).or_else([] {
#ifndef DNDEBUG
        std::wcerr << "Failed to acquire update mutex" << std::endl;
#endif
    });
}
