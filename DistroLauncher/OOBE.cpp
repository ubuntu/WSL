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
#include <algorithm>

namespace DistributionInfo
{

    namespace
    {
        std::wstring PreparePrefillInfo();
        HRESULT OOBEStatusHandling(std::wstring_view status);
        bool EnsureStopped(unsigned int maxNoOfRetries);
        bool mustRunOOBEinTextMode();
        const TCHAR* const OOBE_NAME = L"/usr/libexec/wsl-setup";
    }

    bool shouldSkipInstaller(std::vector<std::wstring_view>& arguments, std::wstring_view value)
    {
        auto it = std::remove(arguments.begin(), arguments.end(), value);
        auto r = std::distance(it, arguments.end());
        arguments.erase(it, arguments.end());
        return r == 0;
    }

    // TODO: Remove this when the rootfs gets the missing groups added.
    void createMissingGroups(std::vector<std::wstring_view> groups)
    {
        constexpr size_t cmdLenght = 80;
        std::wstring command;
        command.reserve(cmdLenght);
        if (groups.empty()) {
            return;
        }

        auto createOneGroup = [&command, wslApi = &g_wslApi](std::wstring_view grp) {
            command.clear();
            command.append(L"groupadd ");
            command.append(grp);
            command.append(L" 2>/dev/null");
            DWORD exitCode;
            // it doesn't really matter if it fails.
            wslApi->WslLaunchInteractive(command.c_str(), FALSE, &exitCode);
        };

        std::for_each(std::begin(groups), std::end(groups), createOneGroup);
    }

    bool isOOBEAvailable()
    {
        DWORD exitCode = -1;
        std::wstring whichCmd{L"which "};
        whichCmd.append(DistributionInfo::OOBE_NAME);
        HRESULT hr = g_wslApi.WslLaunchInteractive(whichCmd.c_str(), TRUE, &exitCode);
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

    // Anonimous namespace to avoid exposing internal details of the implementation.
    namespace
    {

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

            // Write it to a file inside \\wsl.localhost distro filesystem.
            const std::wstring prefillFileNameDest = L"/var/tmp/prefill-system-setup.yaml";
            const std::wstring wslPrefix = L"\\\\wsl.localhost\\" + DistributionInfo::Name;
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

        // OOBEStatusHandling checks the exit status of wsl-setup script
        // and takes the required actions.
        HRESULT OOBEStatusHandling(std::wstring_view status)
        {
            if (status.compare(L"complete") == 0) {
                // Do nothing, just return.
                return S_OK;
            }

            bool needsReboot = (status.compare(L"reboot") == 0);

            // Neither reboot nor shutdown
            if (!needsReboot && (status.compare(L"shutdown") != 0)) {
                return E_INVALIDARG;
            }

            const std::wstring shutdownCmd = L"wsl -t " + DistributionInfo::Name;
            int cmdResult = _wsystem(shutdownCmd.c_str());
            if (cmdResult != 0) {
                return ERROR_FAIL_SHUTDOWN;
            }

            if (!needsReboot) {
                return S_OK;
            }

            // Before relaunching, give WSL some time to make sure
            // distro is stopped.
            bool stopSuccess = EnsureStopped(30); // NOLINT(readability-magic-numbers): only used here.
            if (!stopSuccess) {
                // We could try again, but who knows why
                // we failed to stop the distro in the first time.
                return ERROR_FAIL_SHUTDOWN;
            }

            // We could, but may not want to just `wsl -d Distro`.
            // We can explore running our launcher in the future.
            TCHAR launcherName[MAX_PATH];
            DWORD fnLength = GetModuleFileName(nullptr, launcherName, MAX_PATH);
            if (fnLength == 0) {
                return HRESULT_FROM_WIN32(GetLastError());
            }

            cmdResult = _wsystem(launcherName);
            if (cmdResult != 0) {
                return ERROR_FAIL_RESTART;
            }

            return S_OK;
        } // HRESULT OOBEStatusHandling(std::wstring_view status).

        // Polls WSL to ensure the distro is actually stopped.
        bool EnsureStopped(unsigned int maxNoOfRetries)
        {
            for (unsigned int i = 0; i < maxNoOfRetries; ++i) {
                auto runner = Helpers::ProcessRunner(L"wsl -l --quiet --running");
                auto exitCode = runner.run();
                if (exitCode != 0L) {
                    // I'm sure we will want to customize those messages in the short future.
                    Helpers::PrintErrorMessage(ERROR_FAIL_SHUTDOWN);
                    return false;
                }

                // Returns true once we don't find the DistributionInfo::Name in process's stdout
                auto output = runner.getStdOut();
                if (output.find(DistributionInfo::Name) == std::wstring::npos) {
                    return true;
                }

                // We don't need to be hard real time precise.
                Sleep(997); // NOLINT(readability-magic-numbers): only used here.
            }
            return false;
        }

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
    } // namespace.

} // namespace DistributionInfo.
