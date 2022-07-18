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
#define ARG_EXT_ENABLE_INSTALLER  L"--ui="
#define ARG_EXT_INSTALLER_GUI     ARG_EXT_ENABLE_INSTALLER L"gui"
#define ARG_EXT_INSTALLER_TUI     ARG_EXT_ENABLE_INSTALLER L"tui"
#define ARG_EXT_DISABLE_INSTALLER ARG_EXT_ENABLE_INSTALLER L"none"
#define ARG_EXT_AUTOINSTALL       L"--autoinstall"
#define ARG_EXT_UAP10_PARAMETERS  L"--hide-console"

namespace Oobe::internal
{
    static constexpr std::array allExtendedArgs{ARG_EXT_AUTOINSTALL, ARG_EXT_DISABLE_INSTALLER, ARG_EXT_INSTALLER_GUI,
                                                ARG_EXT_INSTALLER_TUI, ARG_EXT_UAP10_PARAMETERS};
    // compile-time array concatenation.
    template <typename T, std::size_t SizeA, std::size_t SizeB>
    constexpr std::array<T, SizeA + SizeB> join(const std::array<T, SizeA>& left, const std::array<T, SizeB>& right)
    {
        std::array<T, SizeA + SizeB> result;
        for (int i = 0; i < SizeA; ++i) {
            result[i] = left[i];
        }
        for (int i = 0; i < SizeB; ++i) {
            result[i + SizeA] = right[i];
        }
        return result;
    }
    // Types resulting of the extended command line parsing.
    struct AutoInstall
    {
        static constexpr std::array<std::wstring_view, 3> requirements{L"install", ARG_EXT_AUTOINSTALL, L""};
        std::filesystem::path autoInstallFile;
    };
    struct SkipInstaller
    {
        static constexpr std::array<std::wstring_view, 1> requirements{ARG_EXT_DISABLE_INSTALLER};
    };
    struct OobeGui
    {
        static constexpr std::array<std::wstring_view, 1> requirements{ARG_EXT_INSTALLER_GUI};
    };
    struct OobeTui
    {
        static constexpr std::array<std::wstring_view, 1> requirements{ARG_EXT_INSTALLER_TUI};
    };
    struct ManifestMatchedInstall // matches the invocation declared in the .appxmanifest.
    {
        static constexpr std::array<std::wstring_view, 1> requirements{ARG_EXT_UAP10_PARAMETERS};
    };
    struct InstallDefault
    {
        static constexpr std::array<std::wstring_view, 0> requirements{};
    };
    struct InstallOnlyDefault
    {
        static constexpr std::array<std::wstring_view, 1> requirements{L"install"};
    };
    template <typename InstallerOption> struct InteractiveInstallOnly : InstallOnlyDefault
    {
        static constexpr auto requirements = join(InstallOnlyDefault::requirements, InstallerOption::requirements);
    };
    template <typename InstallerOption> struct InteractiveInstallShell : InstallerOption
    {
        using InstallerOption::requirements;
    };
    struct Reconfig
    {
        static constexpr std::array<std::wstring_view, 1> requirements{L"config"};
    };

    // InteractiveInstallOnly<SkipInstaller> and InteractiveInstallShell<SkipInstaller> are actually std::monostate,
    // i.e. both go to upstream resolution.
    // clang-format off
    // [clang-format appears to have trouble with long declarations like the following.]
    using Opts = std::variant<std::monostate, // The upstream command line parsing effects.
                              AutoInstall,
                              ManifestMatchedInstall,
                              InstallDefault,
                              InstallOnlyDefault,
                              InteractiveInstallOnly<OobeGui>,
                              InteractiveInstallOnly<OobeTui>,
                              InteractiveInstallShell<OobeGui>,
                              InteractiveInstallShell<OobeTui>,
                              Reconfig>;
    // clang-format on
    /// Parses a vector of command line arguments according to the extended command line options declared above. The
    /// extended options are removed from the original vector as a side effect to avoid confusing the "upstream command
    /// line parsing" routines. Notice that argv[0], i.e. the program name, is presumed not to be part of the arguments
    /// vector. See DistroLauncher.cpp main() function.
    Opts parseExtendedOptions(std::vector<std::wstring_view>& arguments);

    /// Returns true if the command line parsing result is invalid for the case when the extended CLI is unsupported
    /// (the case for older releases).
    inline bool shouldWarnUnsupported(const Opts& options)
    {
        // [ARG_EXT_UAP10_PARAMETERS] must be accepted in the old releases, even though it will be jsut discarded,
        // thus we should not warn for this case. The same stands for the empty CLI case.
        bool noWarnIf = std::holds_alternative<ManifestMatchedInstall>(options) ||
                        std::holds_alternative<InstallDefault>(options) ||
                        std::holds_alternative<std::monostate>(options);

        return !noWarnIf;
    }
}
