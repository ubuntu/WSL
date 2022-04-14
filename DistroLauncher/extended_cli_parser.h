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
#pragma once
#define ARG_EXT_ENABLE_INSTALLER  L"--ui="
#define ARG_EXT_INSTALLER_GUI     ARG_EXT_ENABLE_INSTALLER L"gui"
#define ARG_EXT_INSTALLER_TUI     ARG_EXT_ENABLE_INSTALLER L"tui"
#define ARG_EXT_DISABLE_INSTALLER ARG_EXT_ENABLE_INSTALLER L"none"
#define ARG_EXT_AUTOINSTALL       L"--autoinstall"

namespace Oobe::internal
{
    static constexpr std::array allExtendedArgs{ARG_EXT_AUTOINSTALL, ARG_EXT_DISABLE_INSTALLER, ARG_EXT_INSTALLER_GUI,
                                                ARG_EXT_INSTALLER_TUI};
    // compile-time array concatenation.
    template <typename T, std::size_t SizeA, std::size_t SizeB>
    constexpr std::array<T, SizeA + SizeB> join(const std::array<T, SizeA>& a, const std::array<T, SizeB>& b)
    {
        std::array<T, SizeA + SizeB> result;
        for (int i = 0; i < SizeA; ++i) {
            result[i] = a[i];
        }
        for (int i = 0; i < SizeB; ++i) {
            result[i + SizeA] = b[i];
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
    using Opts = std::variant<std::monostate, AutoInstall, InstallDefault, InstallOnlyDefault,
                              InteractiveInstallOnly<OobeGui>, InteractiveInstallOnly<OobeTui>,
                              InteractiveInstallShell<OobeGui>, InteractiveInstallShell<OobeTui>, Reconfig>;

    /// Parses a vector of command line arguments according to the extended command line options declared above. The
    /// extended options are removed from the original vector as a side effect to avoid confusing the "upstream command
    /// line parsing" routines. Notice that argv[0], i.e. the program name, is presumed not to be part of the arguments
    /// vector. See DistroLauncher.cpp main() function.
    Opts parseExtendedOptions(std::vector<std::wstring_view>& arguments);
}
