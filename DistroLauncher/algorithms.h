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
    // Checks if a type is in a specified list of types
    template <typename T, typename... Args> constexpr bool is_same_as_any()
    {
        return (std::is_same_v<T, Args> || ...);
    }
    
    // Tells chars appart from other types
    template <typename T> constexpr bool is_character() noexcept
    {
        return is_same_as_any<T, char, wchar_t, signed char, unsigned char, char16_t, char32_t>();
        // C++20 introduces char8_t
    }

    /**
     * This template converts:
     * - T[N] into basic_string_view<T> (works for non-chars too!)
     * - CharT* into basic_string_view<CharT>
     * - Anything else stays the same
     */
    template <typename T> constexpr T const& view_adaptor(T const& value)
    {
        return value;
    }

    template <std::size_t Size, typename T>
    constexpr std::enable_if_t<!is_character<T>(), std::basic_string_view<T>> view_adaptor(const T value[Size])
    {
        // In c++20, switch to std::span
        return std::basic_string_view<T>(value, Size);
    }

    template <typename CharT>
    constexpr std::enable_if_t<is_character<CharT>(), std::basic_string_view<CharT>> view_adaptor(const CharT* value)
    {
        return {value};
    }

    template <typename IteratorTested, typename IteratorCompare>
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) - Parameters must have the same type.
    bool starts_with_impl(IteratorTested tested_begin, IteratorTested tested_end, IteratorCompare compare_begin,
                          IteratorCompare compare_end)
    {
        using tested_type = typename std::iterator_traits<IteratorTested>::value_type;
        using compare_type = typename std::iterator_traits<IteratorCompare>::value_type;

        if constexpr (is_character<tested_type>() || is_character<compare_type>()) {
            static_assert(std::is_same_v<tested_type, compare_type>,
                          "Comparison of diferent width characters is not allowed.");
        }

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
    auto const& tested_ = algo_internal::view_adaptor(tested);
    auto const& compared_ = algo_internal::view_adaptor(start);
    return algo_internal::starts_with_impl(std::cbegin(tested_), std::cend(tested_), std::cbegin(compared_),
                                           std::cend(compared_));
}

/// Returns true if [tested] ends with [end].
template <typename RangeTested, typename RangeCompare>
bool ends_with(RangeTested const& tested, RangeCompare const& end)
{
    auto const& tested_ = algo_internal::view_adaptor(tested);
    auto const& compared_ = algo_internal::view_adaptor(end);
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
