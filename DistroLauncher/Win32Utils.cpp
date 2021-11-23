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

namespace Win32Utils {
    nonstd::expected<std::wstring, std::wstring> utf8_to_wide_string(const std::string& utf8str){
		if (utf8str.empty()){
			return L"";
		}

		const auto size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8str.at(0),
													  static_cast<int>(utf8str.size()), nullptr, 0);
		if (size_needed <= 0){
			nonstd::make_unexpected(L"MultiByteToWideChar() failed. Win32 error code: "
                                     + std::to_wstring(GetLastError()));
		}

		std::wstring result(size_needed, 0);
		auto convResult = MultiByteToWideChar(CP_UTF8, 0, &utf8str.at(0), static_cast<int>(utf8str.size()),
							&result.at(0), size_needed);
        if(convResult==0){
            nonstd::make_unexpected(L"MultiByteToWideChar() failed. Win32 error code: "
                                     + std::to_wstring(GetLastError()));
        }
		return result;
	}

	nonstd::expected<std::string, std::wstring> wide_string_to_utf8(const std::wstring& wide_str){
		if (wide_str.empty()){
			return "";
		}

		const auto size_needed = WideCharToMultiByte(CP_UTF8, 0, &wide_str.at(0),
													static_cast<int>(wide_str.size()),
													nullptr, 0, nullptr, nullptr);
		if (size_needed <= 0){
			nonstd::make_unexpected(L"WideCharToMultiByte() failed. Win32 error code: "
                                     + std::to_wstring(GetLastError()));
		}

		std::string result(size_needed, 0);
		auto convResult = WideCharToMultiByte(CP_UTF8, 0, &wide_str.at(0), static_cast<int>(wide_str.size()),
							&result.at(0), size_needed, nullptr, nullptr);

        if(convResult==0){
            nonstd::make_unexpected(L"WideCharToMultiByte() failed. Win32 error code: "
                                     + std::to_wstring(GetLastError()));
        }
		return result;
	}
} // namespace Win32Utils
