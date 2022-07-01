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

#include "DistributionInfo.h"

// Replace with std::wstring_view::starts_with and ends_with in C++20
[[nodiscard]] bool starts_with(std::wstring_view str, std::wstring_view pattern);
[[nodiscard]] bool ends_with(std::wstring_view str, std::wstring_view pattern);

[[nodiscard]] std::wstring_view GetDefaultUpgradePolicy();

void OverrideUpgradePolicy();

void AfterInstall();