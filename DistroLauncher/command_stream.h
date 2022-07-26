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

#pragma once

#include <sstream>
#include <utility>
#include <cassert>

// WSL API
#include "sudo.h"

/**
 * Abstract
 * --------
 * Syntax sugar to succintly launch WSL commands.
 *
 * Motivation
 * ----------
 * In many parts of the code you can find the following snippet, or similar:
 * ```c++
 * std::wstring_stream cmd;
 * cmd << "Assembling " << the::command << std::quoted(here);
 * DWORD errCode;
 * const HRESULT hr = g_wslApi.LaunchInteractive(cmd.string().c_str(), FALSE, &errCode);
 * if(SUCCEEDED(hr) && errCode == 0) {
 *      return std::nullopt;                        // Or whatever;
 * }
 * ```
 * This is seven lines of code for what should take much fewer. This class proposes the
 * following syntax:
 * ```c++
 * if(WslStream{} << << "Assembling " << the::command << here << WslStream::Call << WslStream::Ok)
 * {
 *      return std::nullopt;                        // Or whatever;
 * }
 * ```
 *
 * Usage
 * -----
 * You can chain any objects that have a `ostream& operator<<(ostream&, object const&) implemented.
 * ```c++
 * WslStream{} << "echo My favourite number is " << 11;
 * ```
 * No spaces are inserted inbetween, so ensure that you add them in. This is useful to chain strings
 * that you want to keep toghether:
 * ```
 * WslStream{} << "ls /usr/bin/" << app_name;
 * ```
 *
 * In order to perform certain actions you can use tags. (Similar to using std::endl or std::flush with std::cout).
 * For instance, tag Call is the most useful; as it executes the command:
 * ```
 * WslStream{} << "echo My favourite number is " << 11 << WslStream::Call;
 * ```
 * The other tags are used to return certain values:
 * - WslStream::String  Returns the command as a wstring.
 * - WslStream::Call    Executes the command. Returns the command as it was. Enables and updates the next four.
 * - WslStream::HResult Returns the HRESULT of the last call. Result undefined if used before Call.
 * - WslStream::ErrCode Returns the linux exit code of the last call. Result undefined if used before Call.
 * - WslStream::Status  Returns the previous two in a tuple. Result undefined if used before Call.
 * - WslStream::Ok      Returns true if both HRESULT and exitCode are succesful. Result undefined if used before Call.
 *
 * See examples in the unit tests.
 *
 */

namespace CommandStreamInternals
{

    template <typename BackEnd> struct CommandStream
    {
        CommandStream() = default;

        CommandStream(CommandStream const& other) :
            use_cwd_{other.use_cwd_}, error_code_{other.error_code_}, hr_{other.hr_}, data_{}
        {
            data_ << other.data_.str();
        }

        CommandStream(CommandStream&& other) = default;

        void call()
        {
            hr_ = BackEnd::LaunchInteractive(data_.str().c_str(), use_cwd_, &error_code_);
        }

        bool ok() const noexcept
        {
            assert(hr_ != S_FALSE);
            return SUCCEEDED(hr_) && error_code_ == 0;
        }

        HRESULT result() const noexcept
        {
            assert(hr_ != S_FALSE);
            return hr_;
        }

        DWORD error_code() const noexcept
        {
            assert(hr_ != S_FALSE);
            return error_code_;
        }

        std::wstring string() const
        {
            return data_.str();
        }

      private:
        /* Tags
         * ----
         *
         * They are passed via operator<< to signal certain actions or to return certain values.
         * They must have a static private member function template called impl.
         * Their type name is in ALL_CAPS. Their instance is named with CamelCase.
         *
         * ```
         * template<typename Self>
         * static RETURN_TYPE impl(Self self) {
         *     // implementation
         * }
         * ```
         *
         * The "self" parameter will be called with:
         *  - CmdInterface &
         *  - CmdInterface const&
         *  - CmdInterface &&
         */

        struct OK
        {
            template <typename Self> static auto impl(Self self) -> bool
            {
                return self.ok();
            }
        };

        struct HR
        {
            template <typename Self> static auto impl(Self self) -> HRESULT
            {
                return self.result();
            }
        };

        struct ERR
        {
            template <typename Self> static auto impl(Self self) -> DWORD
            {
                return self.error_code();
            }
        };

        struct HR_ERR
        {
            template <typename Self> static auto impl(Self self)
            {
                return std::make_tuple(self.result(), self.error_code());
            }
        };

        struct CALL
        {
            template <typename Self> static auto impl(Self self) -> Self
            {
                self.call();
                return std::forward<Self>(self);
            }
        };

        struct STRING
        {
            template <typename Self> static auto impl(Self self) -> std::wstring
            {
                return self.string();
            }
        };

      public:
        // Tag instances
        static constexpr OK Ok{};
        static constexpr HR HResult{};
        static constexpr ERR ErrCode{};
        static constexpr HR_ERR Status{};
        static constexpr CALL Call{};
        static constexpr STRING String{};

        // Piping tags

// We need all three versions for template argument deduction to work properly
#define WSL_COMMAND_STREAM_IMPLEMENT_TAG(tag)                                                                          \
    friend auto operator<<(CommandStream& self, tag)                                                                   \
    {                                                                                                                  \
        return tag::template impl<CommandStream&>(self);                                                               \
    }                                                                                                                  \
    friend auto operator<<(CommandStream const& self, tag)                                                             \
    {                                                                                                                  \
        return tag::template impl<CommandStream const&>(self);                                                         \
    }                                                                                                                  \
    friend auto operator<<(CommandStream&& self, tag)                                                                  \
    {                                                                                                                  \
        return tag::template impl<CommandStream&&>(std::forward<CommandStream>(self));                                 \
    }

        WSL_COMMAND_STREAM_IMPLEMENT_TAG(OK)
        WSL_COMMAND_STREAM_IMPLEMENT_TAG(HR)
        WSL_COMMAND_STREAM_IMPLEMENT_TAG(ERR)
        WSL_COMMAND_STREAM_IMPLEMENT_TAG(HR_ERR)
        WSL_COMMAND_STREAM_IMPLEMENT_TAG(CALL)
        WSL_COMMAND_STREAM_IMPLEMENT_TAG(STRING)

        // Piping commands
        friend std::wostream& operator<<(std::wostream& lhs, CommandStream const& rhs)
        {
            return lhs << rhs.string();
        }

        friend std::wostream&& operator<<(std::wostream& lhs, CommandStream&& rhs)
        {
            return lhs << std::move(rhs.data_).str();
        }

        // Piping string-like objects
        template <typename WStringLike> CommandStream& operator<<(WStringLike& str)
        {
            data_ << str;
            return *this;
        }

        template <typename WStringLike> CommandStream& operator<<(WStringLike&& str)
        {
            data_ << std::forward<WStringLike>(str);
            return *this;
        }

        BOOL use_cwd_ = FALSE;

      private:
        DWORD error_code_ = 0;
        HRESULT hr_ = S_FALSE;
        std::wstringstream data_{};
    };

} // CmdInterfaceInternals

using WslStream = CommandStreamInternals::CommandStream<SudoInternals::WslWindowsAPI>;
