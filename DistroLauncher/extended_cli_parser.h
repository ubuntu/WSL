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
// TODO: Remove those includes when this file gets into stdafx.h
#include <filesystem>
#include <optional>
#include <variant>
#include <algorithm>

#define ARG_EXT_ENABLE_INSTALLER L"--enable-installer"
#define ARG_EXT_AUTOINSTALL      L"--autoinstall"

namespace Oobe::internal
{
    // Types resulting of the extended command line parsing.
    struct AutoInstall
    {
        static constexpr std::array<std::wstring_view, 3> requirements{L"install", ARG_EXT_AUTOINSTALL, L""};
        std::filesystem::path autoInstallFile;
    };
    struct InteractiveInstallOnly
    {
        static constexpr std::array<std::wstring_view, 2> requirements{L"install", ARG_EXT_ENABLE_INSTALLER};
    };
    struct InteractiveInstallShell
    {
        static constexpr std::array<std::wstring_view, 1> requirements{ARG_EXT_ENABLE_INSTALLER};
    };
    struct Reconfig
    {
        static constexpr std::array<std::wstring_view, 1> requirements{L"config"};
    };
    using Opts = std::variant<std::monostate, AutoInstall, InteractiveInstallOnly, InteractiveInstallShell, Reconfig>;

    /// Parses a vector of command line arguments according to the extended command line options declared above. The
    /// extended options are removed from the original vector as a side effect to avoid confusing the "upstream command
    /// line parsing" routines. Notice that argv[0], i.e. the program name, is presumed not to be part of the arguments
    /// vector. See DistroLauncher.cpp main() function.
    Opts parseExtendedOptions(std::vector<std::wstring_view>& arguments);
}
