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
    data = std::vector<std::wstring>{};
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

        data.push_back(std::move(buffer));
        buffer.clear();
    }
}

void PatchLog::write()
{
    DWORD exitCode;
    std::wstringstream command;
    command << L"echo \"# WSL patches log. Do not modify this file.\n";
    std::for_each(data.cbegin(), data.cend(), [&](auto patch) { command << patch << '\n'; });
    command << "\" > " << linux_path;

    WslLaunchInteractiveAsRoot(command.str().c_str(), 1, &exitCode);
}

void PatchLog::push_back(std::wstring_view patchname)
{
    data.emplace_back(patchname);
};

bool PatchLog::contains(std::wstring_view patchname) const
{
    return std::find(data.cbegin(), data.cend(), patchname) != data.cend();
}

bool ApplyPatch(std::wstring_view patchname)
{
    namespace fs = std::filesystem;

    const auto patch_path = (fs::path{patches_windows_path} += patchname) += L".diff";

    HRESULT status;
    RootSession sudo{&status};

    if (FAILED(status)) {
        return false;
    }

    bool success = std::filesystem::copy_file(patch_path, Oobe::WindowsPath(L"/tmp/wslpatch.diff"),
                                              fs::copy_options::overwrite_existing);
    if (!success) {
        return false;
    }

    DWORD errorCode;
    const HRESULT hr = g_wslApi.WslLaunchInteractive(L"patch -ruN < /tmp/wslpatch.diff", 1, &errorCode);

    return SUCCEEDED(hr) && errorCode == 0;
}

[[nodiscard]] std::vector<std::wstring> PatchList()
{
    return {};
}

void ApplyPatches()
{
    PatchLog patch_log{patches_log_linux_path};
    patch_log.read();

    for (const auto& patchname : PatchList()) {

        if (patch_log.contains(patchname)) {
            continue;
        }

        const bool status = ApplyPatch(patchname);
        if (!status) {
            break;
        }

        patch_log.push_back(patchname);
    }

    patch_log.write();
}
