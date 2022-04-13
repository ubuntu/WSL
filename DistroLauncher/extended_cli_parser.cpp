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
#include "extended_cli_parser.h"

namespace Oobe::internal
{
    // Currently all CLI argument parsing outputs can be constructed in the same way, except for the AutoInstall,
    // which requires a file path, thus templates with an specialization for the AutoInstall case. Those templates are
    // only visible and accessible in this file.
    template <typename ParsedOpt> std::optional<ParsedOpt> tryParse(const std::vector<std::wstring_view>& arguments)
    {
        if constexpr (std::is_same_v<ParsedOpt, DEFAULT_EMPTY_CLI_CASE>) {
            if (arguments.size() == 0) {
                return DEFAULT_EMPTY_CLI_CASE{};
            }
        }
        if (arguments.size() != ParsedOpt::requirements.size()) {
            return std::nullopt;
        }
        auto mismatch = std::mismatch(std::begin(ParsedOpt::requirements), std::end(ParsedOpt::requirements),
                                      std::begin(arguments), std::end(arguments));
        if (mismatch.first == ParsedOpt::requirements.end() && mismatch.second == arguments.end()) {
            return ParsedOpt{};
        }

        return std::nullopt;
    }

    template <> std::optional<AutoInstall> tryParse(const std::vector<std::wstring_view>& arguments)
    {
        if (arguments.size() != AutoInstall::requirements.size()) {
            return std::nullopt;
        }
        auto mismatch = std::mismatch(std::begin(AutoInstall::requirements), std::end(AutoInstall::requirements),
                                      std::begin(arguments), std::end(arguments));
        // a mismatch is expected at the last position of the vector, so the iterators must be `previous to end()`.
        if (mismatch.first == std::prev(AutoInstall::requirements.end()) &&
            mismatch.second == std::prev(arguments.end())) {
            return AutoInstall{*(mismatch.second)};
        }

        return std::nullopt;
    }

    Opts parse(const std::vector<std::wstring_view>& arguments)
    {
        // launcher.exe install --autoinstall <autoinstallfile>
        if (auto result = tryParse<AutoInstall>(arguments); result.has_value()) {
            return result.value();
        }

        // launcher.exe - Runs the OOBE (auto detect graphics support) and brings the shell at the end.
        // launcher.exe --installer=gui - Same as above.
        if (auto result = tryParse<InteractiveInstallShell<OobeGui>>(arguments); result.has_value()) {
            return result.value();
        }

        // launcher.exe --installer=tui - Runs the OOBE (forces TUI) and brings the shell at the end.
        if (auto result = tryParse<InteractiveInstallShell<OobeTui>>(arguments); result.has_value()) {
            return result.value();
        }

        // launcher.exe --installer=none - Upstream minimal setup experience with the shell in the end.
        if (auto result = tryParse<InteractiveInstallShell<SkipInstaller>>(arguments); result.has_value()) {
            return std::monostate{};
        }

        // launcher.exe install - Runs the OOBE (auto detect graphics support) and quits.
        // launcher.exe install --installer=gui - Same as above.
        if (auto result = tryParse<InteractiveInstallOnly<OobeGui>>(arguments); result.has_value()) {
            return result.value();
        }

        // launcher.exe install --installer=tui - Runs the OOBE (forces TUI) and quits.
        if (auto result = tryParse<InteractiveInstallOnly<OobeTui>>(arguments); result.has_value()) {
            return result.value();
        }

        // launcher.exe install --installer=none - Upstream minimal setup experience and quit.
        if (auto result = tryParse<InteractiveInstallOnly<SkipInstaller>>(arguments); result.has_value()) {
            return std::monostate{};
        }

        // launcher.exe config - Runs the OOBE in reconfiguration mode.
        if (auto result = tryParse<Reconfig>(arguments); result.has_value()) {
            return result.value();
        }

        // Any other combination of parameters - should delegate to the "upstream command line parsing".
        return std::monostate{};
    }

    Opts parseExtendedOptions(std::vector<std::wstring_view>& arguments)
    {
        Opts options{parse(arguments)};
        // Erasing the extended command line options to avoid confusion in the upstream code.
        auto it = std::remove_if(arguments.begin(), arguments.end(), [](auto arg) {
            return std::find(allExtendedArgs.begin(), allExtendedArgs.end(), arg) != allExtendedArgs.end();
        });
        arguments.erase(it, arguments.end());
        return options;
    }
}