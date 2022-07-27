#pragma once
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

// Common algorithms to be used everywhere in the launcher project with style resembling the std ones.

template <typename CharT>
bool starts_with(const std::basic_string_view<CharT> tested, const std::basic_string_view<CharT> start)
{
    if (tested.size() < start.size()) {
        return false;
    }
    auto mismatch = std::mismatch(start.cbegin(), start.cend(), tested.cbegin());
    return mismatch.first == start.cend();
}

template <typename CharT, std::size_t TestedSize, std::size_t StartSize>
bool starts_with(CharT const (&tested)[TestedSize], CharT const (&start)[StartSize])
{
    return starts_with<CharT>(std::basic_string_view<CharT>{tested}, std::basic_string_view<CharT>{start});
}

template <typename CharT>
bool ends_with(const std::basic_string_view<CharT> tested, const std::basic_string_view<CharT> end)
{
    if (tested.size() < end.size()) {
        return false;
    }
    auto mismatch = std::mismatch(end.crbegin(), end.crend(), tested.crbegin());
    return mismatch.first == end.crend();
}

template <typename CharT, std::size_t TestedSize, std::size_t EndSize>
bool ends_with(const CharT (&tested)[TestedSize], const CharT (&end)[EndSize])
{
    return ends_with<CharT>(std::basic_string_view<CharT>{tested}, std::basic_string_view<CharT>{end});
}

template <typename... Args> std::wstring concat(Args&&... args)
{
    std::wstringstream buffer;
    (buffer << ... << std::forward<Args>(args));
    return buffer.str();
}
