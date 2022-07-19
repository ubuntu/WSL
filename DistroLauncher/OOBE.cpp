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

    HRESULT OOBESetup()
    {
        // Prepare prefill information to send to the OOBE.
        std::wstring prefillCLIPostFix = DistributionInfo::PreparePrefillInfo();
        std::wstring commandLine{L"sudo "};
        commandLine.append(DistributionInfo::OOBE_NAME + prefillCLIPostFix);
        // OOBE runs GUI by default, unless arg --text is set.
        if (mustRunOOBEinTextMode()) {
            commandLine.append(L" --text");
        }
        // calling the OOBE.
        DWORD exitCode = -1;
        HRESULT hr = g_wslApi.WslLaunchInteractive(commandLine.c_str(), TRUE, &exitCode);
        if ((FAILED(hr)) || (exitCode != 0)) {
            return hr;
        }

        hr = g_wslApi.WslLaunchInteractive(L"clear", TRUE, &exitCode);
        if (FAILED(hr)) {
            return hr;
        }

        Oobe::ExitStatusHandling();

        return S_OK;
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

    // Returns true if OOBE has to be launched in text mode.
    // That might be the case due lack of graphics support in WSL
    // or user requirement, by setting the environment variable
    // LAUNCHER_FORCE_MODE, which can only be:
    //	0 or unset or invalid = autodetection
    //	1 = text mode
    //	2 = GUI mode.
    bool mustRunOOBEinTextMode()
    {
        // has to consider the NULL-terminating.
        const DWORD expectedSize = 2;
        wchar_t value[expectedSize];
        auto readResult = GetEnvironmentVariable(L"LAUNCHER_FORCE_MODE", value, expectedSize);
        // var unset is not an error.
        const bool unset = (readResult == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND);
        // more than one char + NULL, that's invalid. Like a string or a more than one digit number.
        const bool notASingleChar = (readResult >= expectedSize || value[1] != NULL);
        // Handle both in the same way: autodetect.
        if (unset || notASingleChar) {
            return !Helpers::WslGraphicsSupported();
        }

        // Expected result if the env var is correctly set:
        // readResult == 1 && value[1] == NULL && value[0] in (0,1,2).
        switch (value[0]) {
        case L'1': // forced text mode.
            return true;
        case L'2': // forced GUI mode, no autodetection.
            return false;
        case L'0':
        default:
            return !Helpers::WslGraphicsSupported();
        }
    }
} // namespace DistributionInfo.
