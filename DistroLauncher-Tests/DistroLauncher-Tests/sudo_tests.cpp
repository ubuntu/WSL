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
#include "dummy_apis.h"

TEST(SudoTests, MonadicInterface)
{
    Testing::WslMockAPI::reset_mock_distro();
    {
        // Testing success
        bool and_then = false;
        bool or_else = false;
        Testing::Sudo scope_lock = Testing::Sudo().and_then([&] { and_then = true; }).or_else([&] { or_else = true; });
        ASSERT_TRUE(and_then);
        ASSERT_FALSE(or_else);
        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0);

        // Testing failure (fails because it's locked already)
        and_then = false;
        or_else = false;

        auto status = Testing::Sudo::Status ::OK;
        Testing::Sudo().and_then([&] { and_then = true; }).or_else([&] { or_else = true; }).or_else([&](auto why) {
            status = why;
        });

        ASSERT_FALSE(and_then);
        ASSERT_TRUE(or_else);
        ASSERT_EQ(status, Testing::Sudo::Status::FAILED_MUTEX);
        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0);
    }
    ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0xabcdef);
}

TEST(SudoTests, WslInterface)
{
    {
        Testing::WslMockAPI::reset_mock_distro();

        // Testing success
        DWORD exitCode = 0xBAD;
        const auto command1 = L"echo hello, world!";
        auto hr = Testing::Sudo::WslLaunchInteractive(command1, TRUE, &exitCode);

        ASSERT_TRUE(SUCCEEDED(hr));
        ASSERT_EQ(exitCode, 0);
        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0xabcdef);
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 1);
        ASSERT_EQ(Testing::WslMockAPI::command_log.size(), 0);
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.back().command, command1);
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.back().useCurrentWorkingDirectory, TRUE);

        const auto command2 = L"mkdir ${HOME}/desktop/important_stuff";
        int x = 0, y = 0, z = 0;
        HANDLE stdIn = &x;
        HANDLE stdOut = &y;
        HANDLE stdErr = &z;
        HANDLE process = nullptr;
        hr = Testing::Sudo::WslLaunch(command2, FALSE, stdIn, stdOut, stdErr, &process);

        ASSERT_TRUE(SUCCEEDED(hr));
        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0xabcdef);
        ASSERT_EQ(Testing::WslMockAPI::command_log.size(), 1);
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 1);

        ASSERT_EQ(Testing::WslMockAPI::command_log.back().command, command2);
        ASSERT_EQ(Testing::WslMockAPI::command_log.back().useCurrentWorkingDirectory, FALSE);
        ASSERT_EQ(Testing::WslMockAPI::command_log.back().stdIn, stdIn);
        ASSERT_EQ(Testing::WslMockAPI::command_log.back().stdOut, stdOut);
        ASSERT_EQ(Testing::WslMockAPI::command_log.back().stdErr, stdErr);
        ASSERT_EQ(process, Testing::WslMockAPI::mock_process);
    }

    // Testing failure (fails because it's locked already)
    {
        Testing::WslMockAPI::reset_mock_distro();
        auto scope_guard = Testing::Sudo();
        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0);

        // Testing success
        DWORD exitCode = 0xBAD;
        const auto command1 = L"echo hello, world!";
        auto hr = Testing::Sudo::WslLaunchInteractive(command1, TRUE, &exitCode);

        ASSERT_TRUE(FAILED(hr));
        ASSERT_EQ(exitCode, 0xBAD);

        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0);
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 0);
        ASSERT_EQ(Testing::WslMockAPI::command_log.size(), 0);

        const auto command2 = L"mkdir ${HOME}/desktop/important_stuff";
        int x = 0, y = 0, z = 0;
        HANDLE stdIn = &x;
        HANDLE stdOut = &y;
        HANDLE stdErr = &z;
        HANDLE process = nullptr;
        hr = Testing::Sudo::WslLaunch(command2, FALSE, stdIn, stdOut, stdErr, &process);

        ASSERT_TRUE(FAILED(hr));
        ASSERT_EQ(process, nullptr);

        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0);
        ASSERT_EQ(Testing::WslMockAPI::command_log.size(), 0);
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 0);
    }
    ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0xabcdef);
}

TEST(SudoTests, Exceptions)
{
    Testing::WslMockAPI::reset_mock_distro();
    ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0xabcdef);

    // derived from std::exception
    {
        try {
            Testing::Sudo().and_then([]() { throw std::runtime_error("Hello!"); });
        } catch (std::runtime_error& err) {
            ASSERT_EQ(err.what(), std::string{"Hello!"});
        } catch (int&) {
            FAIL();
        } catch (...) {
            FAIL();
        }

        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0xabcdef);

        std::optional<bool> previous_mutex_released = std::nullopt;
        Testing::Sudo().and_then([&] { previous_mutex_released = {true}; }).or_else([&]() noexcept {
            previous_mutex_released = {false};
        });

        ASSERT_TRUE(previous_mutex_released.has_value());
        ASSERT_TRUE(previous_mutex_released.value());
    }

    // some other type
    {
        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0xabcdef);
        try {
            Testing::Sudo().and_then([]() { throw 42; });
        } catch (std::runtime_error&) {
            FAIL();
        } catch (int& err) {
            ASSERT_EQ(err, 42);
        } catch (...) {
            FAIL();
        }

        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0xabcdef);

        std::optional<bool> previous_mutex_released = std::nullopt;
        Testing::Sudo().and_then([&] { previous_mutex_released = {true}; }).or_else([&]() noexcept {
            previous_mutex_released = {false};
        });

        ASSERT_TRUE(previous_mutex_released.has_value());
        ASSERT_TRUE(previous_mutex_released.value());
    }
}