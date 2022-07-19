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

TEST(SudoTests, MonadicInterface)
{
    Testing::WslMockAPI::reset_mock_distro();
    {
        // Testing success
        bool and_then = false;
        bool or_else = false;
        Testing::Sudo().and_then([&] { and_then = true; }).or_else([&] { or_else = true; });
        ASSERT_TRUE(and_then);
        ASSERT_FALSE(or_else);
        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0xabcdef); // User restored
    }
    {
        Testing::Sudo scope_lock;
        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0); // User changed to root

        // Testing failure (fails because it's locked already)
        bool and_then = false;
        bool or_else = false;

        auto status = Testing::Sudo::Status::OK;
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
    // Testing success
    {
        Testing::WslMockAPI::reset_mock_distro();

        DWORD exitCode = 0xBAD;
        const auto command1 = L"echo hello, world!";
        auto hr = Testing::Sudo::WslLaunchInteractive(command1, TRUE, &exitCode);

        ASSERT_TRUE(SUCCEEDED(hr));
        ASSERT_EQ(exitCode, 0);
        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0xabcdef);             // Default user restored
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 1); // Interactive command launched
        ASSERT_EQ(Testing::WslMockAPI::command_log.size(), 0);             // Regular command not launched
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.back().command, command1);
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.back().useCurrentWorkingDirectory, TRUE);

        const auto command2 = L"mkdir ${HOME}/desktop/important_stuff";
        int x = 0, y = 0, z = 0;
        HANDLE stdIn = &x;  // Mock StdIn
        HANDLE stdOut = &y; // Mock StdOut
        HANDLE stdErr = &z; // Mock stdErr
        HANDLE process = nullptr;
        hr = Testing::Sudo::WslLaunch(command2, FALSE, stdIn, stdOut, stdErr, &process);

        ASSERT_TRUE(SUCCEEDED(hr));
        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0xabcdef);             // Default user restored
        ASSERT_EQ(Testing::WslMockAPI::command_log.size(), 1);             // Command launched
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 1); // Interactive command not launched

        ASSERT_EQ(Testing::WslMockAPI::command_log.back().command, command2);
        ASSERT_EQ(Testing::WslMockAPI::command_log.back().useCurrentWorkingDirectory, FALSE);
        ASSERT_EQ(Testing::WslMockAPI::command_log.back().stdIn, stdIn);
        ASSERT_EQ(Testing::WslMockAPI::command_log.back().stdOut, stdOut);
        ASSERT_EQ(Testing::WslMockAPI::command_log.back().stdErr, stdErr);
        ASSERT_EQ(process, Testing::WslMockAPI::mock_process); // Process out variable not ignored
    }

    // Testing failure (fails because it's locked already)
    {
        Testing::WslMockAPI::reset_mock_distro();
        auto scope_guard = Testing::Sudo();
        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0);

        DWORD exitCode = 0xBAD;
        const auto command1 = L"echo hello, world!";
        auto hr = Testing::Sudo::WslLaunchInteractive(command1, TRUE, &exitCode);

        ASSERT_TRUE(FAILED(hr));    // Failed to acquire Sudo lock
        ASSERT_EQ(exitCode, 0xBAD); // Exit code not modified

        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0);                    // Still sudo
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 0); // Command not lauched
        ASSERT_EQ(Testing::WslMockAPI::command_log.size(), 0);             // Command not lauched

        const auto command2 = L"mkdir ${HOME}/desktop/important_stuff";
        int x = 0, y = 0, z = 0;
        HANDLE stdIn = &x;  // Mock StdIn
        HANDLE stdOut = &y; // Mock StdOut
        HANDLE stdErr = &z; // Mock stdErr
        HANDLE process = nullptr;
        hr = Testing::Sudo::WslLaunch(command2, FALSE, stdIn, stdOut, stdErr, &process);

        ASSERT_TRUE(FAILED(hr));     // Failed to acquire Sudo lock
        ASSERT_EQ(process, nullptr); // Process not assigned

        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0);                    // Still sudo
        ASSERT_EQ(Testing::WslMockAPI::command_log.size(), 0);             // Command not lauched
        ASSERT_EQ(Testing::WslMockAPI::interactive_command_log.size(), 0); // Command not lauched
    }
    ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0xabcdef); // Scope lock released
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

        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0xabcdef); // User restored after throwing

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

        ASSERT_EQ(Testing::WslMockAPI::defaultUID_, 0xabcdef); // User restored after throwing

        std::optional<bool> previous_mutex_released = std::nullopt;
        Testing::Sudo().and_then([&] { previous_mutex_released = {true}; }).or_else([&]() noexcept {
            previous_mutex_released = {false};
        });

        ASSERT_TRUE(previous_mutex_released.has_value());
        ASSERT_TRUE(previous_mutex_released.value());
    }
}
