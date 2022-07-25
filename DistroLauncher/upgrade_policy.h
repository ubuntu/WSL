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

namespace internal
{
    bool starts_with(std::wstring_view tested, std::wstring_view start);
    bool ends_with(std::wstring_view tested, std::wstring_view end);

    std::wstring GetUpgradePolicy();

    template <typename... Args> std::wstring concat(Args&&... args)
    {
        std::wstringstream buffer;
        (buffer << ... << std::forward<Args>(args));
        return buffer.str();
    }

    void SetDefaultUpgradePolicyImpl();

}

void SetDefaultUpgradePolicy();
