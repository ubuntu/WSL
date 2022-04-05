#include "stdafx.h"
#include "gtest/gtest.h"
#include "extended_cli_parser.cpp"

namespace Oobe::internal
{
    TEST(ExtendedCliParserTests, AutoInstallGoodCLI)
    {
        const wchar_t path[]{L"~/Downloads/autoinstall.yaml"};
        std::vector<std::wstring_view> args{L"install", ARG_EXT_AUTOINSTALL, path};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<AutoInstall>(opts));
        auto& opt = std::get<AutoInstall>(opts);
        ASSERT_TRUE(opt.autoInstallFile == path);
    }

    TEST(ExtendedCliParserTests, AutoInstallMissingDashesIsFailure)
    {
        const wchar_t path[]{L"~/Downloads/autoinstall.yaml"};
        std::vector<std::wstring_view> args{L"install", L"autoinstall", path};
        auto opts = parseExtendedOptions(args);
        ASSERT_FALSE(std::holds_alternative<AutoInstall>(opts));
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
    }

    TEST(ExtendedCliParserTests, AutoInstallMissingPathResultsNone)
    {
        std::vector<std::wstring_view> args{L"install", ARG_EXT_AUTOINSTALL};
        auto opts = parseExtendedOptions(args);
        ASSERT_FALSE(std::holds_alternative<AutoInstall>(opts));
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
    }

    TEST(ExtendedCliParserTests, InstallOobeNoShell)
    {
        std::vector<std::wstring_view> args{L"install", ARG_EXT_ENABLE_INSTALLER};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<InteractiveInstallOnly>(opts));
    }
    TEST(ExtendedCliParserTests, InstallOobeWithShell)
    {
        std::vector<std::wstring_view> args{ARG_EXT_ENABLE_INSTALLER};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<InteractiveInstallShell>(opts));
    }
    TEST(ExtendedCliParserTests, OobeReconfig)
    {
        std::vector<std::wstring_view> args{L"config"};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<Reconfig>(opts));
    }
    // Tests whether the upstream options remain preserved:
    TEST(ExtendedCliParserUpstreamPreservedTests, InstallOobeDisabledWithShell)
    {
        std::vector<std::wstring_view> args{};
        auto opts = parseExtendedOptions(args);
        ASSERT_TRUE(std::holds_alternative<std::monostate>(opts));
    }
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