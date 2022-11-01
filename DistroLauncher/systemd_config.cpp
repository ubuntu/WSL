#include "stdafx.h"

namespace Systemd
{

    // Appends text to file in the distro. Returns a success boolean.
    bool AppendToFile(std::wstring const& text, std::filesystem::path const& file)
    {
        const auto command = concat(L"printf ", std::quoted(text), L" >> ", file);
        DWORD exitCode;
        const HRESULT hr = g_wslApi.WslLaunchInteractive(command.c_str(), FALSE, &exitCode);
        return SUCCEEDED(hr) && exitCode == 0;
    }

    // Calls mkdir. Returns a success boolean.
    bool Mkdir(std::wstring_view flags, std::filesystem::path const& linux_path)
    {
        const auto command = concat(L"mkdir ", flags, L" ", linux_path);
        DWORD exitCode;
        const HRESULT hr = g_wslApi.WslLaunchInteractive(command.c_str(), FALSE, &exitCode);
        return SUCCEEDED(hr) && exitCode == 0;
    }

    // Enables or disables systemd in config file
    bool EnableSystemd()
    {
        const std::filesystem::path wsl_conf{L"/etc/wsl.conf"};
        return AppendToFile(L"\n[boot]\nsystemd=true\n", wsl_conf);
    }

    /**
     * Overrides LoadCredential setting for systemd-sysusers.service
     * See related bug report: https://bugs.launchpad.net/ubuntu/+source/lxd/+bug/1950787
     * See related solution: https://git.launchpad.net/~ubuntu-core-dev/ubuntu/+source/systemd/commit/?id=d4e05ecbc
     */
    bool SysUsersDisableLoadCredential()
    {
        const std::filesystem::path sysusers_override = L"/etc/systemd/system/systemd-sysusers.service.d/override.conf";

        if (!Mkdir(L"-p", sysusers_override.parent_path())) {
            return false;
        }
        return AppendToFile(L"\n[Service]\nLoadCredential=\n", sysusers_override);
    }

    void Configure(const bool enable)
    {
        SysUsersDisableLoadCredential();

        if (!enable) {
            return;
        }

        if (!EnableSystemd()) {
            return;
        }

        AppendToFile(L"\naction=reboot\n", L"/run/launcher-command");
    }
}
