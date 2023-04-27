/*
 * Copyright (C) 2023 Canonical Ltd
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
#include "PatchConfig.h"

namespace Ubuntu
{

    Patcher::Patcher(const std::filesystem::path& pathPrefix, const std::filesystem::path& linuxFile)
    {
        // Path concatenation may surprise us if the linuxFile has a root component:
        // \\$wsl\Distro + /etc/fstab = \\$wsl/etc/fstab (latest root overrides previous).
        if (linuxFile.has_root_directory()) {
            translatedFilePath = pathPrefix / linuxFile.relative_path(); // relative to root path.
        } else {
            translatedFilePath = pathPrefix / linuxFile;
        }
        // converts the directory separators to the preferred by the platform. Not stricly necessary, but it's good for
        // observability.
        translatedFilePath.make_preferred();
    }

    bool Patcher::commit()
    {
        std::error_code err;
        // like `mkdir -p`: existing directories won't set the error output parameter.
        std::filesystem::create_directories(translatedFilePath.parent_path(), err);
        if (err) {
            // directory cannot exist.
            return false;
        }
        // Opening in binary mode to prevent translating \n into \r\n.
        std::ofstream file{translatedFilePath, std::ios::binary | std::ios::trunc};
        if (file.fail()) {
            return false;
        }

        modified.seekg(0);
        // istreambuf_iterator doesn't skip whitespaces and new lines. Only EOF can stop it.
        std::copy(std::istreambuf_iterator<char>(modified), std::istreambuf_iterator<char>{},
                  std::ostreambuf_iterator<char>(file));
        return !file.fail();
    }
}
