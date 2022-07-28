/*
 * Copyright (C) 2021 Canonical Ltd
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

namespace DistributionInfo
{

    namespace
    {
        const TCHAR* const OOBE_NAME = L"/usr/libexec/wsl-setup";
    }

    bool shouldSkipInstaller(std::vector<std::wstring_view>& arguments, std::wstring_view value)
    {
        const auto new_end = std::remove(arguments.begin(), arguments.end(), value);
        const auto n_removed = std::distance(new_end, arguments.end());
        arguments.erase(new_end, arguments.end());
        return n_removed == 0;
    }

    bool isOOBEAvailable()
    {
        DWORD exitCode = -1;
        std::wstring whichCmd{L"which "};
        whichCmd.append(DistributionInfo::OOBE_NAME);
        HANDLE child = nullptr;
        HRESULT hr = g_wslApi.WslLaunch(whichCmd.c_str(), FALSE, nullptr, nullptr, nullptr, &child);
        WaitForSingleObject(child, INFINITE);
        GetExitCodeProcess(child, &exitCode);
        // true if launching the process succeeds and `which` command returns 0.
        return ((SUCCEEDED(hr)) && (exitCode == 0));
    }

    // Saves Windows information inside Linux filesystem to supply to the OOBE.
    // Returns empty string if we fail to generate the prefill file
    // or the postfix to be added to the OOBE command line.
    std::wstring PreparePrefillInfo()
    {
        std::wstring commandLine;
        const auto prefillInfo = DistributionInfo::GetPrefillInfoInYaml();
        if (prefillInfo.empty()) {
            return commandLine;
        }

        // Write it to a file inside the distribution filesystem.
        const std::wstring prefillFileNameDest = L"/var/tmp/prefill-system-setup.yaml";
        const std::wstring wslPrefix = Oobe::WslPathPrefix() + DistributionInfo::Name;
        std::ofstream prefillFile;
        // Mixing slashes and backslashes that way is not a problem for Windows.
        prefillFile.open(wslPrefix + prefillFileNameDest, std::ios::ate);
        if (prefillFile.fail()) {
            Helpers::PrintErrorMessage(CO_E_FAILEDTOCREATEFILE);
            return commandLine;
        }

        prefillFile << prefillInfo;
        prefillFile.close();
        if (prefillFile.fail()) {
            Helpers::PrintErrorMessage(CO_E_FAILEDTOCREATEFILE);
            return commandLine;
        }

        commandLine += L" --prefill=" + prefillFileNameDest;
        return commandLine;
    } // std::wstring PreparePrefillInfo().

} // namespace DistributionInfo.
