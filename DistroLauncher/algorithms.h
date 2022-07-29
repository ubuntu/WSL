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

/// Common algorithms to be used everywhere in the launcher project with style resembling the std ones.

/// Returns true if [tested] starts with [end]. Null-termination is not required.
template <typename CharT>
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) - Parameters must have the same type.
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

/// Returns true if [tested] ends with [end]. Null-termination is not required.
template <typename CharT>
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) - Parameters must have the same type.
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

/// Returns true for the first entry of [directory] for which [pred] returns true.
/// Returns false if none of the entries match the predicate.
/// Iteration order is not specified.
template <typename Pred> bool any_file_of(const std::filesystem::path& directory, Pred&& pred)
{
    namespace fs = std::filesystem;
    std::error_code error;
    assert(fs::is_directory(directory));
    auto listing = fs::directory_iterator{directory, error};
    if (error) {
        return false;
    }
    return std::find_if(begin(listing), end(listing), std::forward<Pred>(pred)) != end(listing);
}

// Pushes back to the vector [v] as many elements to a vector as [args] count by folding subsequent calls to
// v.push_back().
template <typename T, typename... Args> void push_back_many(std::vector<T>& vec, Args&&... args)
{
    static_assert((std::is_constructible_v<T, Args&&> && ...));
    (vec.push_back(std::forward<Args>(args)), ...);
}
