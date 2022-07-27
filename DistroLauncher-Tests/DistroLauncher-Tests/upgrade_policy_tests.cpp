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

TEST(UpgradePolicyTests, StringParsing)
{
    ASSERT_FALSE(starts_with(L"", L"hello"));
    ASSERT_FALSE(starts_with(L"he", L"hello"));
    ASSERT_TRUE(starts_with(L"hello", L"hello"));
    ASSERT_TRUE(starts_with(L"hello, world!", L"hello"));
    ASSERT_FALSE(starts_with(L"cheers, world!", L"hello"));
    ASSERT_FALSE(starts_with(L"HELLO", L"hello"));

    ASSERT_FALSE(ends_with(L"", L"world!"));
    ASSERT_FALSE(ends_with(L"d!", L"world!"));
    ASSERT_TRUE(ends_with(L"world!", L"world!"));
    ASSERT_TRUE(ends_with(L"hello, world!", L"world!"));
    ASSERT_FALSE(ends_with(L"hello, world?", L"world!"));
    ASSERT_FALSE(ends_with(L"this string is completely diferent", L"world!"));
    ASSERT_FALSE(ends_with(L"HELLO", L"hello"));

    ASSERT_FALSE(starts_with(L"", L"Ubuntu"));
    ASSERT_TRUE(starts_with(L"Ubuntu 22.04.1 LTS", L"Ubuntu"));
    ASSERT_TRUE(ends_with(L"Ubuntu 22.04.1 LTS", L"LTS"));
}

TEST(UpgradePolicyTests, Concat)
{
    // Checking default functionality
    std::wstring dog = L"dog";
    auto example = concat(L"The", L" quick brown fox", L" jumps over the lazy ", std::quoted(dog), '.');

    ASSERT_EQ(example, LR"(The quick brown fox jumps over the lazy "dog".)");

    // Checking quote nesting
    auto nested = concat(L"\n{\n  ", std::quoted(L"name"), L": ", std::quoted(L"example"), L",\n  ",
                         std::quoted(L"value"), L": ", std::quoted(example), L"\n}\n");
    auto expected = LR"(
{
  "name": "example",
  "value": "The quick brown fox jumps over the lazy \"dog\"."
}
)";
    ASSERT_EQ(nested, expected);

    // Checking path auto-quoting
    std::filesystem::path example_file{L"/home/fox/documents/example.json"};
    auto with_path = concat(L"diff ", example_file, L" ", example_file.wstring()); // Only first one to be quoted
    ASSERT_EQ(with_path, LR"(diff "/home/fox/documents/example.json" /home/fox/documents/example.json)");
}
