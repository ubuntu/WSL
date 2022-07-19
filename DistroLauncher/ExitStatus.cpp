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
#include <filesystem>

namespace Helpers
{
    // TODO: find a better place for this function. Inside Win32Utils, probably. Useful for dealing with std:exception.
    void PrintFromUtf8(const char* msg)
    {
        const size_t MAX_MSG_LENGTH = 256;
        const int size = static_cast<int>(strnlen_s(msg, MAX_MSG_LENGTH));
        const auto size_needed = MultiByteToWideChar(CP_UTF8, 0, msg, size, nullptr, 0);
        if (size_needed <= 0) {
            Helpers::PrintMessage(MSG_ERROR_CODE, L"Failed with unknown error message");
            return;
        }

        std::wstring result(size_needed, 0);
        auto convResult = MultiByteToWideChar(CP_UTF8, 0, msg, size, &result.at(0), size_needed);
        if (convResult == 0) {
            Helpers::PrintMessage(MSG_ERROR_CODE, L"Failed with unknown error message");
            return;
        }
        Helpers::PrintMessage(MSG_ERROR_CODE, result.c_str());
    }
}

namespace Oobe
{

    namespace
    {
        const wchar_t* const launcherCommandFilePath{L"/run/launcher-command"};
        VoidResult act(const KeyValuePairs& cmds);
        VoidResult config(const KeyValuePairs& cmds);
    }

    void ExitStatusHandling()
    {
        std::wstring prefixedFilePath{WslPathPrefix()};
        prefixedFilePath.append(DistributionInfo::Name);
        prefixedFilePath.append(launcherCommandFilePath);
        std::ifstream launcherCmdFile;
        if (!std::filesystem::exists(prefixedFilePath)) {
            // OOBE left nothing to do.
            return;
        }
        launcherCmdFile.open(prefixedFilePath);
        if (launcherCmdFile.fail()) {
            Helpers::PrintErrorMessage(HRESULT_FROM_WIN32(GetLastError()));
            return;
        }

        auto parserResult = internal::parseExitStatusFile(launcherCmdFile);
        if (parserResult.has_value()) {
            // Finally take the actions and delete the file.
            const KeyValuePairs& launcherCmds{parserResult.value()};
            auto configResult = config(launcherCmds);
            if (!configResult.has_value()) {
                Helpers::PrintFromUtf8(configResult.error().what());
            }
            auto actionResult = act(launcherCmds);
            if (!actionResult.has_value()) {
                Helpers::PrintFromUtf8(actionResult.error().what());
            }
        } else {
            wprintf(parserResult.error());
        }

        launcherCmdFile.close();
        // We don't want that file existing after actions were taken.
        std::filesystem::remove(prefixedFilePath);
    }

    namespace
    {
        KeyValuePairs launcherCmds;
        const unsigned int MAX_NUMBER_OF_ATTEMPTS = 30;

        // Shall we need to extend the capabilities triggered by the launcher-command file, here is what we need to do:
        // 1. Add more functions inside the Actions namespace with the same signature of the existing ones.
        // 2. Add those functions associated with the expected value in the file which will trigger them to the
        // capabilities constant map.
        // 3. If that requires extending the parser grammar, see `exit_status_parser.cpp`
        namespace Actions
        {
            VoidResult rebootDistro();
            VoidResult shutdownDistro();
        };
        using Action = VoidResult (*)();
        const std::unordered_map<std::string_view, Action> capabilities{{"reboot", &Actions::rebootDistro},
                                                                        {"shutdown", &Actions::shutdownDistro}};

        // Polls WSL to ensure the distro is actually stopped.
        bool ensureDistroStopped(unsigned int maxNoOfRetries)
        {
            for (unsigned int i = 0; i < maxNoOfRetries; ++i) {
                auto runner = Helpers::ProcessRunner(L"wsl -l --quiet --running");
                auto exitCode = runner.run();
                if (exitCode != 0L) {
                    return false;
                }

                // Returns true once we don't find the DistributionInfo::Name in process's stdout
                auto output = runner.getStdOut();
                auto distroNameLine = DistributionInfo::Name + L'\r';
                if (output.find(distroNameLine) == std::wstring::npos) {
                    return true;
                }

                // We don't need to be hard real time precise.
                Sleep(997); // NOLINT(readability-magic-numbers): only used here.
            }
            return false;
        }

        VoidResult act(const KeyValuePairs& launcherCmds)
        {
            const auto actionIt = launcherCmds.find("action");
            if (actionIt == launcherCmds.end()) {
                // nothing to do.
                return VoidResult();
            }

            // I'm hardcoding that "action" expects strings.
            const auto action = std::any_cast<std::string>(actionIt->second);

            const auto found = capabilities.find(action);
            if (found == capabilities.end()) {
                return nonstd::make_unexpected(std::runtime_error(action));
            }

            auto res = found->second();
            if (!res.has_value()) {
                return nonstd::make_unexpected(res.error());
            }

            return VoidResult();
        }

        // Since there is only one kind configuration entry expected at this moment, it would not make a lot of sense to
        // create enums or tables to work with that.
        VoidResult config(const KeyValuePairs& launcherCmds)
        {
            auto configIt = launcherCmds.find("defaultUid");
            if (configIt == launcherCmds.end()) {
                // nothing to do
                return VoidResult();
            }
            auto default_uid = std::any_cast<unsigned long>(configIt->second);
            // TODO: Replace the default flags on this call by the current ones reading before writing.
            // The planned wrapper class should resolve this.
            auto hr = g_wslApi.WslConfigureDistribution(default_uid, WSL_DISTRIBUTION_FLAGS_DEFAULT);
            if (FAILED(hr)) {
                return nonstd::make_unexpected(std::runtime_error(
                  "Could not configure distro to the new default UID: " + std::to_string(default_uid)));
            }

            return VoidResult();
        }

        namespace Actions
        {
            VoidResult rebootDistro()
            {
                const std::wstring shutdownCmd = L"wsl -t " + DistributionInfo::Name;
                int cmdResult = _wsystem(shutdownCmd.c_str());
                if (cmdResult != 0) {
                    return nonstd::make_unexpected(std::runtime_error("Failed to invoke shutdown command."));
                }

                // Before relaunching, give WSL some time to make sure distro is stopped.
                bool stopSuccess = ensureDistroStopped(MAX_NUMBER_OF_ATTEMPTS);
                if (!stopSuccess) {
                    // We could try again, but why would we have failed to stop the distro in the first time?
                    return nonstd::make_unexpected(std::runtime_error("Distro is still running after wsl -t timeout."));
                }

                return VoidResult();
            }

            VoidResult shutdownDistro()
            {
                // There is no proper shutdown semantics in WSL.
                // The upstream code will restart the distro if shell is required.
                // Unless we call exit(0) from here, which sounds dramatic and unnecessary.
                return rebootDistro();
            }
        } // namespace Actions

    }; // namespace

} // namespace Oobe