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

#pragma once

namespace Oobe
{
    // Opens the /run/launcher-command file left by the OOBE (currently), calls the parser and takes the actions
    // referred in the file.
    void ExitStatusHandling();

    namespace internal
    {
        // Parses the file left by OOBE (for now) with actions to be taken upon its end of
        // execution, like shutdown or reboot the distro, set the default UID thru the WSL API. That file has a very
        // simple structure having just key-value pairs with a very strict grammar. Any line not complying with that
        // grammar is silently ignored. In the end, if the data structured filed by the parsing is empty, a failure
        // message ready to print to console is returned instead of the expected key-value pairs.
        nonstd::expected<KeyValuePairs, const wchar_t*> parseExitStatusFile(std::istream& file);
    }
}
