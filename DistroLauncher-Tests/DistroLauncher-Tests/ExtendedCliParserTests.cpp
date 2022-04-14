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
        // launcher.exe install --ui=gui
        std::vector<std::wstring_view> args{L"install", ARG_EXT_INSTALLER_GUI};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<InteractiveInstallOnly<OobeGui>>(opts));
    }

    TEST(ExtendedCliParserTests, InstallOobeWithShellGui)
    {
        // launcher.exe --ui=gui
        std::vector<std::wstring_view> args{ARG_EXT_INSTALLER_GUI};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<InteractiveInstallShell<OobeGui>>(opts));
    }

    TEST(ExtendedCliParserTests, InstallOobeNoShellTui)
    {
        // launcher.exe install --ui=tui
        std::vector<std::wstring_view> args{L"install", ARG_EXT_INSTALLER_TUI};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<InteractiveInstallOnly<OobeTui>>(opts));
    }

    TEST(ExtendedCliParserTests, InstallOobeWithShellTui)
    {
        // launcher.exe install --ui=tui
        std::vector<std::wstring_view> args{ARG_EXT_INSTALLER_TUI};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<InteractiveInstallShell<OobeTui>>(opts));
    }

    TEST(ExtendedCliParserTests, SkipInstallerNoShell)
    {
        // launcher.exe install --ui=none
        std::vector<std::wstring_view> args{L"install", ARG_EXT_DISABLE_INSTALLER};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
    }

    TEST(ExtendedCliParserTests, SkipInstallerWithShell)
    {
        // launcher.exe --ui=none
        std::vector<std::wstring_view> args{ARG_EXT_DISABLE_INSTALLER};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
    }

    TEST(ExtendedCliParserTests, BrokenOptionGoesUpstream1)
    {
        // [X] launcher.exe install --ui
        std::vector<std::wstring_view> args{L"install", L"--ui"};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
    }

    TEST(ExtendedCliParserTests, BrokenOptionGoesUpstream2)
    {
        // [X] launcher.exe --ui
        std::vector<std::wstring_view> args{L"--ui"};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
    }

    // This also used to be upstream, but now is OOBE
    TEST(ExtendedCliParserTests, InstallOnlyOobeNoShell)
    {
        // launcher.exe install
        std::vector<std::wstring_view> args{L"install"};
        auto previousSize = args.size();
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<InstallOnlyDefault>(opts));
        ASSERT_EQ(previousSize, args.size());
    }

    TEST(ExtendedCliParserTests, OobeReconfig)
    {
        // launcher.exe config
        std::vector<std::wstring_view> args{L"config"};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<Reconfig>(opts));
    }

    // used to be upstream minimal setup experience, but it's now OOBE GUI.
    TEST(ExtendedCliParserTests, DefaultEmptyCase)
    {
        std::vector<std::wstring_view> args{};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<InstallDefault>(opts));
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