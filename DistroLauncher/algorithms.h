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

namespace algo_internal
{
    // Tells chars appart from other types
    template <typename T> constexpr bool is_character() noexcept
    {
        return std::is_same_v<T, char> || std::is_same_v<T, wchar_t> || std::is_same_v<T, signed char> ||
               std::is_same_v<T, unsigned char> || std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>;
        // C++20 introduces char8_t
    }

    /**
     * This template converts c-strings into string_views, and
     * leaves all other types untouched
     */
    template <typename T> constexpr T&& string_adaptor(T&& value)
    {
        return std::forward<T>(value);
    }

    template <typename T> constexpr T const& string_adaptor(T const& value)
    {
        return value;
    }

    template <typename T> constexpr T& string_adaptor(T& value)
    {
        return value;
    }

    template <typename T> constexpr std::enable_if_t<!is_character<T>(), T*> string_adaptor(T* value)
    {
        return value;
    }

    template <typename T> constexpr std::enable_if_t<!is_character<T>(), T const*> string_adaptor(T* value)
    {
        return value;
    }

    template <typename Char>
    constexpr std::enable_if_t<is_character<Char>(), std::basic_string_view<Char>> string_adaptor(Char* value)
    {
        return {value};
    }

    template <typename Char>
    constexpr std::enable_if_t<is_character<Char>(), std::basic_string_view<const Char>> string_adaptor(
      Char const* value)
    {
        return {value};
    }

    template <typename IteratorTested, typename IteratorCompare>
    bool starts_with_impl(IteratorTested tested_begin, IteratorTested tested_end, IteratorCompare compare_begin,
                          IteratorCompare compare_end)
    {
        const auto tested_size = std::distance(tested_begin, tested_end);
        const auto compare_size = std::distance(compare_begin, compare_end);
        if (tested_size < compare_size) {
            return false;
        }
        auto mismatch = std::mismatch(compare_begin, compare_end, tested_begin);
        return mismatch.first == compare_end;
    }

}

/// Returns true if [tested] starts with [start].
template <typename RangeTested, typename RangeCompare>
bool starts_with(RangeTested const& tested, RangeCompare const& start)
{
    auto const& tested_ = algo_internal::string_adaptor(tested);
    auto const& compared_ = algo_internal::string_adaptor(start);
    return algo_internal::starts_with_impl(std::cbegin(tested_), std::cend(tested_), std::cbegin(compared_),
                                           std::cend(compared_));
}

/// Returns true if [tested] ends with [end].
template <typename RangeTested, typename RangeCompare>
bool ends_with(RangeTested const& tested, RangeCompare const& end)
{
    auto const& tested_ = algo_internal::string_adaptor(tested);
    auto const& compared_ = algo_internal::string_adaptor(end);
    return algo_internal::starts_with_impl(std::crbegin(tested_), std::crend(tested_), std::crbegin(compared_),
                                           std::crend(compared_));
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
    vec.reserve(vec.size() + sizeof...(args));
    (vec.push_back(std::forward<Args>(args)), ...);
}
