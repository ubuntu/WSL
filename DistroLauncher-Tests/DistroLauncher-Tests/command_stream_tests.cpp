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

#include "stdafx.h"
#include "gtest/gtest.h"
#include "mock_api.h"

TEST(CommandStreamTests, Call)
{
    Testing::WslMockAPI::reset_mock_distro();

    // Not calling a command
    {
        auto cmd = Testing::WslStream{} << L"echo Just a simple command";
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 0u);
        ASSERT_EQ(cmd.string(), L"echo Just a simple command");
    }

    // Calling a command
    {
        auto cmd = Testing::WslStream{} << L"echo Calling a simple command" << Testing::WslStream::Call;
        ASSERT_EQ(cmd.string(), L"echo Calling a simple command");
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 1u);
        ASSERT_EQ(Testing::WslMockAPI::latest_command(), L"echo Calling a simple command");
    }

    // Concatenating commands
    {
        auto cmd = Testing::WslStream{} << L"echo " << L"Calling a simple command" << Testing::WslStream::Call;
        ASSERT_EQ(cmd.string(), L"echo Calling a simple command");
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 2u);
        ASSERT_EQ(Testing::WslMockAPI::latest_command(), L"echo Calling a simple command");
    }

    // Multiple launches
    {
        auto cmd = Testing::WslStream{} << L"cat /home/john_doe/.bashrc";
        ASSERT_EQ(cmd.string(), L"cat /home/john_doe/.bashrc");
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 2u);

        cmd << Testing::WslStream::Call;
        ASSERT_EQ(cmd.string(), L"cat /home/john_doe/.bashrc");
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 3u);
        ASSERT_EQ(Testing::WslMockAPI::latest_command(), L"cat /home/john_doe/.bashrc");

        cmd << Testing::WslStream::Call;
        ASSERT_EQ(cmd.string(), L"cat /home/john_doe/.bashrc");
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 4u);
        ASSERT_EQ(Testing::WslMockAPI::latest_command(), L"cat /home/john_doe/.bashrc");

        cmd << " 2> /dev/null" << Testing::WslStream::Call;
        ASSERT_EQ(cmd.string(), L"cat /home/john_doe/.bashrc 2> /dev/null");
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 5u);
        ASSERT_EQ(Testing::WslMockAPI::latest_command(), L"cat /home/john_doe/.bashrc 2> /dev/null");
    }
}

TEST(CommandStreamTests, StatusTags)
{
    Testing::WslMockAPI::reset_mock_distro();

    // Getting String
    {
        auto str = Testing::WslStream{} << L"echo 'Hello!'" << Testing::WslStream::Call << Testing::WslStream::String;
        ASSERT_EQ(str, L"echo 'Hello!'");
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 1u);
        ASSERT_EQ(Testing::WslMockAPI::latest_command(), L"echo 'Hello!'");
    }

    // Getting HRESULT
    {
        HRESULT hr = Testing::WslStream{} << L"echo 'Hello!'" << Testing::WslStream::Call
                                          << Testing::WslStream::HResult;
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 2u);
        ASSERT_EQ(Testing::WslMockAPI::latest_command(), L"echo 'Hello!'");
        ASSERT_EQ(hr, S_OK);
    }

    // Getting errorCode
    {
        DWORD errCode = Testing::WslStream{} << L"echo 'Hi!'" << Testing::WslStream::Call
                                             << Testing::WslStream::ErrCode;
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 3u);
        ASSERT_EQ(Testing::WslMockAPI::latest_command(), L"echo 'Hi!'");
        ASSERT_EQ(errCode, 0);
    }

    // Getting both
    {
        auto [hr, errCode] = Testing::WslStream{} << L"echo 'Hi!'" << Testing::WslStream::Call
                                                  << Testing::WslStream::Status;
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 4u);
        ASSERT_EQ(Testing::WslMockAPI::latest_command(), L"echo 'Hi!'");
        ASSERT_EQ(hr, S_OK);
        ASSERT_EQ(errCode, 0);
    }

    // Getting boolean
    {
        bool ok = Testing::WslStream{} << L"echo 'Hi!'" << Testing::WslStream::Call << Testing::WslStream::Ok;
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 5u);
        ASSERT_EQ(Testing::WslMockAPI::latest_command(), L"echo 'Hi!'");
        ASSERT_TRUE(ok);
    }
}

TEST(CommandStreamTests, Nesting)
{
    Testing::WslMockAPI::reset_mock_distro();

    // Nesting via .string
    {
        auto inner = Testing::WslStream{} << L"echo Hello";
        auto outter = Testing::WslStream{} << L"bash -ec " << std::quoted(inner.string()) << Testing::WslStream::Call;

        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 1u);
        ASSERT_EQ(Testing::WslMockAPI::latest_command(), LR"(bash -ec "echo Hello")");
    }

    // Raw nesting
    {
        const auto first = Testing::WslStream{} << L"echo Hello";
        const auto second = Testing::WslStream{} << L"date --iso-8601";

        auto combination = Testing::WslStream{} << first << L" && " << second << Testing::WslStream::Call;

        ASSERT_EQ(combination.string(), LR"(echo Hello && date --iso-8601)");
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 2u);
        ASSERT_EQ(Testing::WslMockAPI::latest_command(), LR"(echo Hello && date --iso-8601)");

        ASSERT_EQ(first.string(), L"echo Hello");
        ASSERT_EQ(second.string(), L"date --iso-8601");
    }

    // Raw nesting, non-const
    {
        auto first = Testing::WslStream{} << L"echo Hello";
        auto second = Testing::WslStream{} << L"date --iso-8601";

        auto combination = Testing::WslStream{} << first << L" && " << second << Testing::WslStream::String;

        ASSERT_EQ(combination, LR"(echo Hello && date --iso-8601)");
        ASSERT_EQ(first.string(), L"echo Hello");
        ASSERT_EQ(second.string(), L"date --iso-8601");
    }

    // Move nesting
    {
        auto combination = Testing::WslStream{} << L"echo Today is && " << (Testing::WslStream{} << L"date --iso-8601")
                                                << Testing::WslStream::Call;

        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 3u);
        ASSERT_EQ(Testing::WslMockAPI::latest_command(), LR"(echo Today is && date --iso-8601)");
    }
}

TEST(CommandStreamTests, Paths)
{
    Testing::WslMockAPI::reset_mock_distro();

    // No quotes via .wstring
    {
        const std::filesystem::path path = L"/tmp/remove-me/";

        Testing::WslStream{} << L"rmdir " << path.wstring() << Testing::WslStream::Call;
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 1u);
        ASSERT_EQ(Testing::WslMockAPI::latest_command(), LR"(rmdir /tmp/remove-me/)");
    }

    // Yes quotes
    {
        const std::filesystem::path path = L"/tmp/remove-me/";

        Testing::WslStream{} << L"rmdir " << path << Testing::WslStream::Call;
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 2u);
        ASSERT_EQ(Testing::WslMockAPI::latest_command(), LR"(rmdir "/tmp/remove-me/")");
    }
}

TEST(CommandStreamTests, Numbers)
{
    Testing::WslMockAPI::reset_mock_distro();

    {
        Testing::WslStream{} << L"echo " << 42 << Testing::WslStream::Call;
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 1u);
        ASSERT_EQ(Testing::WslMockAPI::latest_command(), LR"(echo 42)");
    }

    {
        Testing::WslStream{} << L"I had lunch in a " << std::hex << 51966 << Testing::WslStream::Call;
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 2u);
        ASSERT_EQ(Testing::WslMockAPI::latest_command(), LR"(I had lunch in a cafe)");
    }

    {
        Testing::WslStream{} << L"echo " << 6.023 << Testing::WslStream::Call;
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 3u);
        ASSERT_EQ(Testing::WslMockAPI::latest_command(), LR"(echo 6.023)");
    }
}