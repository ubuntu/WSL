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

#if defined _M_ARM64
// The splash application is written in Dart/Flutter, which currently does not supported Windows ARM64 targets.
// See: https://github.com/flutter/flutter/issues/62597

#include "NoSplashStrategy.h"
using DefaultAppStrategy = Oobe::NoSplashStrategy;

#else // _M_ARM64

#include "SplashEnabledStrategy.h"
using DefaultAppStrategy = Oobe::SplashEnabledStrategy;

#endif // _M_ARM64
