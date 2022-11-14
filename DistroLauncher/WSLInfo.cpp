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

namespace Oobe::internal
{
    namespace
    {
        bool isX11UnixSocketMounted();
    }
    // Although currently only used in this translation unit,
    // this function has a lot of potential to not be exported.
    bool WslLaunchSuccess(const TCHAR* command, DWORD dwMilisseconds)
    {
        using defer = std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&::CloseHandle)>;
        DWORD exitCode;
        HANDLE child = nullptr;

        HRESULT hr = g_wslApi.WslLaunch(command, FALSE, nullptr, nullptr, nullptr, &child);
        if (child == nullptr || FAILED(hr)) {
            return false;
        }
        defer processGuard{child, CloseHandle}; // ensures to call CloseHandle in the end of the scope.

        if (WaitForSingleObject(child, dwMilisseconds) == WAIT_OBJECT_0) {
            auto success = GetExitCodeProcess(child, &exitCode);
            return (success == TRUE && exitCode == 0);
        }

        TerminateProcess(child, WAIT_TIMEOUT);
        return false;
    }

    // Retrieves subsystem version from the WSL API.
    DWORD WslGetDistroSubsystemVersion()
    {
        constexpr unsigned short FOURTHBIT = 0x08;
        ULONG distributionVersion;
        ULONG defaultUID;
        WSL_DISTRIBUTION_FLAGS wslDistributionFlags;
        PSTR* defaultEnvironmentVariables;
        ULONG defaultEnvironmentVariableCount;

        HRESULT hr = g_wslApi.WslGetDistributionConfiguration(&distributionVersion,
                                                              &defaultUID,
                                                              &wslDistributionFlags,
                                                              &defaultEnvironmentVariables,
                                                              &defaultEnvironmentVariableCount);
        if (FAILED(hr) || distributionVersion == 0) {
            Helpers::PrintErrorMessage(hr);
            return 0;
        }

        // discarding the env variable string array otherwise they would leak.
        for (int64_t i = defaultEnvironmentVariableCount - 1; i >= 0; --i) {
            CoTaskMemFree(static_cast<LPVOID>(defaultEnvironmentVariables[i]));
        }
        CoTaskMemFree(static_cast<LPVOID>(defaultEnvironmentVariables));

        // Per conversation at https://github.com/microsoft/WSL-DistroLauncher/issues/96
        // the information about version 1 or 2 is on the 4th bit of the distro flags, which is not
        // currently referenced by the API nor docs. The `1+` is just to ensure we return 1 or two.
        return (1 + ((wslDistributionFlags & FOURTHBIT) >> 3));
    }

    inline bool isWslgEnabled()
    {
        return isX11UnixSocketMounted();
    }

    bool isLocalhostForwardingEnabled(const std::filesystem::path& wslConfig)
    {
        if (!std::filesystem::exists(wslConfig)) {
            return true; // enabled by default.
        }

        std::wifstream config;
        config.open(wslConfig.wstring(), std::ios::beg);
        if (config.fail()) {
            return false; // unsure whether the user disabled or not.
        }

        bool isDisabled = ini_find_value(config, L"wsl2", L"localhostforwarding", L"false");
        return !isDisabled;
    }

    namespace
    {
        // Returns true if the WSL successfully launches a list
        // of commands to ensure X11 socket is properly mounted.
        // One indication that the distro has graphical support enabled.
        bool isX11UnixSocketMounted()
        {
            const std::array cmds = {L"ls -l /tmp/.X11-unix",                // symlink is resolved,
                                     L"ss -lx | grep \"/tmp/.X11-unix/X0\"", // socket is listening,
                                     L"test -n $DISPLAY"};                   // display env var is set.

            // I'm sure is better to read this way than with the algorithm.
            // NOLINTNEXTLINE(readability-use-anyofallof)
            for (const auto* const cmd : cmds) {
                if (!WslLaunchSuccess(cmd)) {
                    wprintf(L"Command %s failed.\n", cmd);
                    return false;
                }
            }

            return true;
        }

    } // namespace.

    // Returns true if /etc/wsl.conf file contains the boot command to activate systemd.
    bool is_systemd_enabled()
    {
        std::wstring wslConfPath{WslPathPrefix()};
        wslConfPath.append(DistributionInfo::Name);
        wslConfPath.append(L"/etc/wsl.conf");
        std::wifstream wslConf;
        wslConf.open(wslConfPath, std::ios::in);
        if (wslConf.fail()) {
            return false;
        }
        return ini_find_value(wslConf, L"boot", L"command", L"/usr/libexec/wsl-systemd");
    }

    bool hasSnap(std::wstring_view name)
    {
        namespace fs = std::filesystem;
        const auto path = Oobe::WindowsPath(fs::path{L"/var/lib/snapd/snaps/"});
        return any_file_of(path, [name](const auto& entry) {
            const std::wstring_view filename{entry.path().filename().wstring()};
            return fs::is_regular_file(entry) && starts_with(filename, name) && ends_with(filename, L"snap");
        });
    }

    bool hasUdiSnap()
    {
        static bool hasUdi = hasSnap(L"ubuntu-desktop-installer");

        return hasUdi;
    }

    bool hasSubiquitySnap()
    {
        static bool hasSubiquity = hasSnap(L"subiquity");
        return hasSubiquity;
    }

    bool WslGraphicsSupported()
    {
        // It's possible that we have only Subiquity instead of the Ubuntu-Desktop-Installer snap.
        if (!Oobe::internal::hasUdiSnap()) {
            return false;
        }

        if (!Oobe::internal::isWslgEnabled()) {
            return false;
        }

        // Could WSL 3 or greater exist in the future?
        return Oobe::internal::WslGetDistroSubsystemVersion() > 1;
    }

} // namespace Helpers.

namespace Oobe
{
    /// Returns a possibly wrapped command line to invoke [intendedCommand] if systemd is enabled.
    std::wstring WrapCommand(std::wstring_view intendedCommand)
    {
        if (!internal::is_systemd_enabled()) {
            return std::wstring{intendedCommand};
        }

        std::wstring command{L"/usr/libexec/nslogin "};
        command.append(intendedCommand);
        return command;
    }

    const wchar_t* WslPathPrefix()
    {
        // This seams more readable than a ternary operator expression, specially if future OS versions require a
        // different path prefix.
        static const wchar_t* prefix = [](auto version) {
            switch (version) {
            case Win32Utils::WinVersion::Win10:
                return L"\\\\wsl$\\";
            case Win32Utils::WinVersion::Win11:
                return L"\\\\wsl.localhost\\";
            }
            assert(false && "Unsupported Windows version.");
            __assume(0); // Unreachable
        }(Win32Utils::os_version());

        return prefix;
    }

    HRESULT WslGetDefaultUserAndFlags(ULONG& defaultUID, WSL_DISTRIBUTION_FLAGS& wslDistributionFlags) noexcept
    {
        ULONG distributionVersion;
        PSTR* defaultEnvironmentVariables = nullptr;
        ULONG defaultEnvironmentVariableCount = 0;

        const HRESULT hr =
          g_wslApi.WslGetDistributionConfiguration(&distributionVersion, &defaultUID, &wslDistributionFlags,
                                                   &defaultEnvironmentVariables, &defaultEnvironmentVariableCount);

        if (FAILED(hr) || distributionVersion == 0) {
            return hr;
        }

        // discarding the env variable string array otherwise they would leak.
        for (ULONG i = 0; i < defaultEnvironmentVariableCount; ++i) {
            CoTaskMemFree(static_cast<LPVOID>(defaultEnvironmentVariables[i]));
        }
        CoTaskMemFree(static_cast<LPVOID>(defaultEnvironmentVariables));

        return hr;
    }

    std::filesystem::path WindowsPath(const std::wstring_view distro_path)
    {
        return {Oobe::WslPathPrefix() + DistributionInfo::Name + std::wstring{distro_path}};
    }

    std::filesystem::path WindowsPath(const std::filesystem::path& distro_path)
    {
        return Oobe::WslPathPrefix() + DistributionInfo::Name + distro_path.wstring();
    }

    bool WslFileExists(const std::filesystem::path& distro_path)
    {
        std::error_code err;
        const bool found = std::filesystem::exists(Oobe::WindowsPath(distro_path), err);
        if (!err) {
            return found;
        }

        // Fallback
        const auto cmd = (std::wstringstream{} << L"test -f " << distro_path << L" > /dev/null 2>&1").str();
        DWORD exitCode;
        const HRESULT hr = g_wslApi.WslLaunchInteractive(cmd.c_str(), FALSE, &exitCode);
        if (SUCCEEDED(hr)) {
            return exitCode == 0;
        }
        return false;
    }
}
