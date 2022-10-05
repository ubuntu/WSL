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

namespace Oobe
{
    struct AppConfig
    {
        // The file name (including extension) of the executable that must be invoked as the OOBE.
        wchar_t* appName;

        // Whether or not the fallback method should be skipped on OOBE failure.
        bool mustSkipFallback;

        // Whether the child process requires its own console or not.
        bool requiresNewConsole;
    };

    // Returns the global constant instance of the AppConfig.
    AppConfig appConfig();
}
