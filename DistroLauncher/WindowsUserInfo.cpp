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
namespace Helpers {
	std::string wide_string_to_string(const std::wstring& wide_string);
	std::wstring string_to_wide_string(const std::string& string);
}

// YAML-Cpp knows how to serialize std::map, but we don't need the sorting
// mechanism that container provides, so let's extend that library to support 
// std::unordered_map and other kinds of maps as well.
namespace YAML {
	template <template <typename...> class MapType, typename K, typename V>
	inline Emitter& operator<<(Emitter& emitter, const MapType<K, V>& m) {
		emitter << BeginMap;
		for (const auto& v : m)
			emitter << Key << v.first << Value << v.second;
		emitter << EndMap;
		return emitter;
	}
}

namespace DistributionInfo {
	// Implementation details.
	namespace {
		// WindowsUserInfo holds together the user information retrieved from Win32 API's.
		struct WindowsUserInfo {
			std::wstring userName;
			std::wstring realName;
			std::wstring localeName;

			std::string toYamlUtf8() const;
		};

		// PrintLastError converts the last error code from Win32 API's into
		// an error message.
		inline void PrintLastError() {
			HRESULT error = HRESULT_FROM_WIN32(GetLastError());
			Helpers::PrintErrorMessage(error);
		}

		std::string WindowsUserInfo::toYamlUtf8() const {
			// yaml-cpp doesn't support wide chars.
			std::unordered_map<std::string,
				std::unordered_map<std::string, std::string>> sections;
			if (!localeName.empty()) {
				sections["Welcome"]["lang"] = Helpers::wide_string_to_string(localeName);
			}

			if (!realName.empty()) {
				sections["WSLIdentity"]["realname"] = Helpers::wide_string_to_string(realName);
			}

			if (!userName.empty()) {
				sections["WSLIdentity"]["username"] = Helpers::wide_string_to_string(userName);
			}

			YAML::Emitter out;
			out << sections << YAML::Newline;
			if (!out.good()) {
				return "";
			}

			// Ensure UTF-8 BOM is present just as a precaution.
			std::string retVal{ "\xEF\xBB\xBF" };
			retVal.append(out.c_str());
			return retVal;
		} // std::string WindowsUserInfo::toYamlUtf8()

		// QueryWindowsUserInfo queries Win32 API's to provide launcher with locale, user real and login names.
		// Those pieces of information will be used in the Ubuntu OOBE to enhance the UX.
		WindowsUserInfo QueryWindowsUserInfo()
		{
			DistributionInfo::WindowsUserInfo userInfo;
			const int size = LOCALE_NAME_MAX_LENGTH;
			WCHAR loc[size];

			std::size_t result = GetUserDefaultLocaleName(loc, LOCALE_NAME_MAX_LENGTH);
			// `Prefill` should handle partial data, thus a missing piece of information is not a function failure.
			if (result == 0) {
				PrintLastError();
			} else {
				std::wstring_view view{ loc,result };
				std::size_t dashPos = view.find_first_of('-');
				if (dashPos > 0 && dashPos < result) {
					loc[dashPos] = '_';
				}
				// (result-1) because GetUserDefaultLocaleName includes the Windows null-terminating char, not needed here.
				userInfo.localeName = std::wstring{ loc, result - 1 };
			}

			WCHAR userRealName[size];
			ULONG nSize = size;
			BOOLEAN ret = GetUserNameExW(EXTENDED_NAME_FORMAT::NameDisplay, userRealName, &nSize);
			if (ret == 0) {
				PrintLastError();
			} else {
				userInfo.realName = std::wstring{ userRealName,nSize };
			}
			
			nSize = size;
			ret = GetUserNameExW(EXTENDED_NAME_FORMAT::NameSamCompatible, userRealName, &nSize);
			if (ret == 0) {
				PrintLastError();
			} else {
				std::wstring_view samView{ userRealName,nSize };
				std::wstring_view justTheUserName = samView.substr(samView.find_first_of('\\') + 1, samView.length());
				userInfo.userName = std::wstring{ justTheUserName };
			}

			return userInfo;
		} // WindowsUserInfo QueryWindowsUserInfo().

	} // namespace.

	// GetPrefillInfoInYaml exports Windows User Information as an YAML string.
	// This is the only symbol visible outside of this translation unit.
	std::string GetPrefillInfoInYamlUtf8() {
		WindowsUserInfo userInfo = DistributionInfo::QueryWindowsUserInfo();
		return userInfo.toYamlUtf8();
	} // std::wstring GetPrefillInfoInYaml().

} // namespace DistributionInfo.


namespace Helpers {
// Helper functions to deal with wide-multibyte string conversion,
// needed due the fact that YAML-Cpp doesn't support wide strings,
// which are default on Windows.
	std::wstring string_to_wide_string(const std::string& string)
	{
		if (string.empty())
		{
			return L"";
		}

		const auto size_needed = MultiByteToWideChar(CP_UTF8, 0, &string.at(0),
													  static_cast<int>(string.size()), nullptr, 0);
		if (size_needed <= 0)
		{
			throw std::runtime_error("MultiByteToWideChar() failed: " + std::to_string(size_needed)
									 + ". Error code: " + std::to_string(GetLastError()));
		}

		std::wstring result(size_needed, 0);
		MultiByteToWideChar(CP_UTF8, 0, &string.at(0), static_cast<int>(string.size()),
							&result.at(0), size_needed);
		return result;
	}

	std::string wide_string_to_string(const std::wstring& wide_string)
	{
		if (wide_string.empty())
		{
			return "";
		}

		const auto size_needed = WideCharToMultiByte(CP_UTF8, 0, &wide_string.at(0),
													static_cast<int>(wide_string.size()),
													nullptr, 0, nullptr, nullptr);
		if (size_needed <= 0)
		{
			throw std::runtime_error("WideCharToMultiByte() failed: " + std::to_string(size_needed)
									 + ". Error code: " + std::to_string(GetLastError()));
		}

		std::string result(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, &wide_string.at(0), static_cast<int>(wide_string.size()),
							&result.at(0), size_needed, nullptr, nullptr);
		return result;
	}
} // namespace Helpers.