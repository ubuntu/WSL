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

namespace DistributionInfo
{
    // Implementation details.
    namespace
    {
        // WindowsUserInfo holds together the user information retrieved from Win32 API's.
        struct WindowsUserInfo
        {
            std::wstring userName;
            std::wstring realName;
            std::wstring localeName;

            std::string toYamlUtf8() const;
        };

        // PrintLastError converts the last error code from Win32 API's into
        // an error message.
        inline void PrintLastError()
        {
            HRESULT error = HRESULT_FROM_WIN32(GetLastError());
            wprintf(L"Recovering user information failed with the code: %lx\n", error);
        }

        // toYaml hand-codes the YAML generation to avoid adding a lib just
        // because of such small feature, which would be an overkill. Shall the need
        // for more YAML manipulation in the DistroLauncher arise, thus function should
        // be changed to use a proper YAML manipulation library, such as yaml-cpp.
        std::string WindowsUserInfo::toYamlUtf8() const
        {
            std::wstring fullYaml;

            if (!localeName.empty()) {
                fullYaml += L"Welcome:\n  lang: " + localeName + L'\n';
            }

            if (!realName.empty() || !userName.empty()) {
                fullYaml += L"WSLIdentity:\n";

                if (!realName.empty()) {
                    fullYaml += L"  realname: " + realName + L'\n';
                }

                if (!userName.empty()) {
                    fullYaml += L"  username: " + userName + L'\n';
                }
            }

            auto yamlStr = Win32Utils::wide_string_to_utf8(fullYaml);
            if (!yamlStr.has_value()) {
                // Failure in this case affects UX just a little bit, yet it would be useful to know why.
                wprintf(yamlStr.error().c_str());
                PrintLastError();
                return std::string{};
            }

            return yamlStr.value();
        } // std::wstring WindowsUserInfo::toYamlUtf8()

        std::wstring GetUserDisplayName()
        {
            constexpr auto size = LOCALE_NAME_MAX_LENGTH;
            WCHAR userRealName[size];
            ULONG nSize = size;

            BOOLEAN ret = GetUserNameExW(EXTENDED_NAME_FORMAT::NameDisplay, userRealName, &nSize);
            if (ret != 0) {
                return std::wstring{userRealName, nSize};
            }

            ret = GetUserNameExW(EXTENDED_NAME_FORMAT::NameGivenName, userRealName, &nSize);
            if (ret != 0) {
                std::wstring realName{userRealName, nSize};
                ret = GetUserNameExW(EXTENDED_NAME_FORMAT::NameSurname, userRealName, &nSize);
                if (ret != 0) {
                    realName.append(1, L' ');
                    realName.append(userRealName, nSize);
                }
                return realName;
            }
            PrintLastError();
            return {};
        }

        std::wstring GetUserRealName()
        {
            constexpr auto size = LOCALE_NAME_MAX_LENGTH;
            WCHAR userRealName[size];
            ULONG nSize = size;

            auto ret = GetUserNameExW(EXTENDED_NAME_FORMAT::NameSamCompatible, userRealName, &nSize);
            if (ret == 0) {
                PrintLastError();
                return {};
            }

            std::wstring_view samView{userRealName, nSize};
            std::wstring_view justTheUserName = samView.substr(samView.find_first_of('\\') + 1, samView.length());
            return std::wstring{justTheUserName};
        }

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
                std::wstring_view view{loc, result};
                std::size_t dashPos = view.find_first_of('-');
                if (dashPos > 0 && dashPos < result) {
                    loc[dashPos] = '_';
                }
                // (result-1) because GetUserDefaultLocaleName includes the null-terminating char, not needed here.
                userInfo.localeName = std::wstring{loc, result - 1};
            }

            userInfo.realName = GetUserDisplayName();

            userInfo.userName = GetUserRealName();

            return userInfo;
        } // WindowsUserInfo QueryWindowsUserInfo().

    } // namespace.

    // GetPrefillInfoInYaml exports Windows User Information as an YAML UTF-8
    // encoded string.
    // This is the only symbol visible outside of this translation unit.
    std::string GetPrefillInfoInYaml()
    {
        WindowsUserInfo userInfo = DistributionInfo::QueryWindowsUserInfo();
        return userInfo.toYamlUtf8();
    } // std::wstring GetPrefillInfoInYaml().

} // namespace DistributionInfo.
