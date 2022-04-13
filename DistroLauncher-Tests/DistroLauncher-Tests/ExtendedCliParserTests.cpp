#include "stdafx.h"
#include "gtest/gtest.h"
#include "extended_cli_parser.cpp"

namespace Oobe::internal
{
    TEST(ExtendedCliParserTests, AutoInstallGoodCLI)
    {
        // launcher.exe install --autoinstall <autoinstallfile>
        const wchar_t path[]{L"~/Downloads/autoinstall.yaml"};
        std::vector<std::wstring_view> args{L"install", ARG_EXT_AUTOINSTALL, path};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<AutoInstall>(opts));
        auto& opt = std::get<AutoInstall>(opts);
        ASSERT_TRUE(opt.autoInstallFile == path);
    }

    TEST(ExtendedCliParserTests, AutoInstallMissingDashesIsFailure)
    {
        // [X] launcher.exe install autoinstall <autoinstallfile>
        const wchar_t path[]{L"~/Downloads/autoinstall.yaml"};
        std::vector<std::wstring_view> args{L"install", L"autoinstall", path};
        auto opts = parseExtendedOptions(args);
        ASSERT_FALSE(std::holds_alternative<AutoInstall>(opts));
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
    }

    TEST(ExtendedCliParserTests, AutoInstallMissingPathResultsNone)
    {
        // [X] launcher.exe install --autoinstall
        std::vector<std::wstring_view> args{L"install", ARG_EXT_AUTOINSTALL};
        auto opts = parseExtendedOptions(args);
        ASSERT_FALSE(std::holds_alternative<AutoInstall>(opts));
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
    }

    TEST(ExtendedCliParserTests, InstallOobeNoShellGui)
    {
        // launcher.exe install --installer=gui
        std::vector<std::wstring_view> args{L"install", ARG_EXT_INSTALLER_GUI};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<InteractiveInstallOnly<OobeGui>>(opts));
    }

    TEST(ExtendedCliParserTests, InstallOobeWithShellGui)
    {
        // launcher.exe --installer=gui
        std::vector<std::wstring_view> args{ARG_EXT_INSTALLER_GUI};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<InteractiveInstallShell<OobeGui>>(opts));
    }

    TEST(ExtendedCliParserTests, InstallOobeNoShellTui)
    {
        // launcher.exe install --installer=tui
        std::vector<std::wstring_view> args{L"install", ARG_EXT_INSTALLER_TUI};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<InteractiveInstallOnly<OobeTui>>(opts));
    }

    TEST(ExtendedCliParserTests, InstallOobeWithShellTui)
    {
        // launcher.exe install --installer=tui
        std::vector<std::wstring_view> args{ARG_EXT_INSTALLER_TUI};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<InteractiveInstallShell<OobeTui>>(opts));
    }

    TEST(ExtendedCliParserTests, SkipInstallerNoShell)
    {
        // launcher.exe install --installer=none
        std::vector<std::wstring_view> args{L"install", ARG_EXT_DISABLE_INSTALLER};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
    }

    TEST(ExtendedCliParserTests, SkipInstallerWithShell)
    {
        // launcher.exe --installer=none
        std::vector<std::wstring_view> args{ARG_EXT_DISABLE_INSTALLER};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
    }

    TEST(ExtendedCliParserTests, BrokenOptionGoesUpstream1)
    {
        // [X] launcher.exe install --installer
        std::vector<std::wstring_view> args{L"install", L"--installer"};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
    }

    TEST(ExtendedCliParserTests, BrokenOptionGoesUpstream2)
    {
        // [X] launcher.exe --installer
        std::vector<std::wstring_view> args{L"--installer"};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
    }
    TEST(ExtendedCliParserTests, OobeReconfig)
    {
        // launcher.exe config
        std::vector<std::wstring_view> args{L"config"};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<Reconfig>(opts));
    }

    TEST(ExtendedCliParserUpstreamPreservedTests, DefaultEmptyCase)
    {
        // used to be upstream minimal setup experience, but it's now OOBE GUI.
        std::vector<std::wstring_view> args{};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<DEFAULT_EMPTY_CLI_CASE>(opts));
    }

    // Tests whether the upstream options remain preserved:
    TEST(ExtendedCliParserTests, InstallRootIsUpstream)
    {
        std::vector<std::wstring_view> args{L"install", L"--root"};
        auto previousSize = args.size();
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
        ASSERT_EQ(previousSize, args.size());
    }
    TEST(ExtendedCliParserUpstreamPreservedTests, InstallOobeDisabledNoShell)
    {
        std::vector<std::wstring_view> args{L"install"};
        auto previousSize = args.size();
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
        ASSERT_EQ(previousSize, args.size());
    }
    TEST(ExtendedCliParserUpstreamPreservedTests, ConfigDefaultUserIsUpstream)
    {
        std::vector<std::wstring_view> args{L"config", L"--default-user", L"u"};
        auto previousSize = args.size();
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
        ASSERT_EQ(previousSize, args.size());
    }
    TEST(ExtendedCliParserUpstreamPreservedTests, HelpIsUpstream)
    {
        std::vector<std::wstring_view> args{L"help"};
        auto previousSize = args.size();
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
        ASSERT_EQ(previousSize, args.size());
    }
    TEST(ExtendedCliParserUpstreamPreservedTests, RunIsUpstream)
    {
        std::vector<std::wstring_view> args{L"run", L"whoami"};
        auto previousSize = args.size();
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
        ASSERT_EQ(previousSize, args.size());
    }
    TEST(ExtendedCliParserUpstreamPreservedTests, InvalidIntallIsUpstream)
    {
        std::vector<std::wstring_view> args{L"install", L"--user"};
        auto previousSize = args.size();
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
        ASSERT_EQ(previousSize, args.size());
    }
    TEST(ExtendedCliParserUpstreamPreservedTests, InvalidConfigIsUpstream)
    {
        std::vector<std::wstring_view> args{L"config", L"--boot-command", L"/usr/libexec/wsl-systemd"};
        auto previousSize = args.size();
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
        ASSERT_EQ(previousSize, args.size());
    }
}