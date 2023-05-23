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
#include "Patch.h"

// The meat: the catalog of patches we may need to perform on a distro.
namespace Ubuntu::PatchingFunctions
{
    bool RemoveCloudImgLabel(std::istreambuf_iterator<char> fstab, std::ostream& tmp)
    {
        bool modified = false;
        for (std::string line; fstab != std::istreambuf_iterator<char>{};) {
            fstab = getline(fstab, line);
            auto trimmedLeft = left_trimmed(std::string_view{line});
            // Write line if doesn't contain the offending label.
            if (!starts_with(trimmedLeft, "LABEL=cloudimg-rootfs")) {
                tmp.write(line.c_str(), static_cast<std::streamsize>(line.length()));
                tmp << '\n'; // getline never adds the newline.
                continue;
            }
            modified = true; // only set if we skip writing a line.
        }

        return modified;
    }

    bool OverrideUnitVirtualizationContainer(std::istreambuf_iterator<char> unused, std::ostream& conf)
    {
        conf << "[Unit]\nConditionVirtualization=!container\n";
        return !conf.fail();
    }

    /**
     * Overrides LoadCredential setting for systemd-sysusers.service
     * See related bug report: https://bugs.launchpad.net/ubuntu/+source/lxd/+bug/1950787
     * See related solution: https://git.launchpad.net/~ubuntu-core-dev/ubuntu/+source/systemd/commit/?id=d4e05ecbc
     */
    bool SysUsersDisableLoadCredential(std::istreambuf_iterator<char> input, std::ostream& output)
    {
        std::copy(input, std::istreambuf_iterator<char>{}, std::ostream_iterator<char>(output));
        output << "\n[Service]\nLoadCredential=\n";
        return true;
    }

    bool EnableSystemd(std::istreambuf_iterator<char> input, std::ostream& output)
    {
        std::copy(input, std::istreambuf_iterator<char>{}, std::ostream_iterator<char>(output));
        output << "\n[boot]\nsystemd=true\n";
        return true;
    }

    bool SetDefaultUpgradePolicy(std::istreambuf_iterator<char> input, std::ostream& output)
    {
        // Decide which policy to use
        const auto policy = []() -> std::string {
            std::wstring_view distro = DistributionInfo::WindowTitle;
            if (distro == L"Ubuntu") {
                return "lts";
            }
            if (starts_with(distro, L"Ubuntu") && ends_with(distro, L"LTS")) {
                return "never";
            }
            return "normal";
        }();

        // Copy until "Prompt=" is found
        std::string line;
        std::string_view prefix;
        auto it = getline(input, line);
        for (; it != std::istreambuf_iterator<char>{}; it = getline(it, line)) {
            auto trimmed = left_trimmed(std::string_view{line});
            if (starts_with(trimmed, "Prompt")) {
                prefix = std::string_view(&*line.cbegin(), line.size() - trimmed.size());
                break;
            }
            output << line << '\n';
        }

        // Override prompt
        output << prefix << "Prompt=" << policy << '\n';

        // Copy the rest of the file
        std::copy(it, std::istreambuf_iterator<char>{}, std::ostream_iterator<char>(output));

        return true;
    }

    bool DeferReboot(std::istreambuf_iterator<char> input, std::ostream& output)
    {
        std::copy(input, std::istreambuf_iterator<char>{}, std::ostream_iterator<char>(output));
        output << "\naction=reboot\n";
        return true;
    }
}

namespace Ubuntu
{

    Patch::Patcher::Patcher(const std::filesystem::path& pathPrefix, const std::filesystem::path& linuxFile)
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

    bool Patch::Patcher::handleCall(PatchFn& patchFn)
    {
        std::error_code err;
        // let the destructor take care of closing the file, since we don't commit here.
        std::ifstream original{};
        std::istreambuf_iterator<char> input{}; // EOF by default.

        // File exists - no OS error while checking that.
        if (bool exists = std::filesystem::exists(translatedFilePath, err); !err && exists) {
            original.open(translatedFilePath);
            if (!original.fail()) {
                input = std::istreambuf_iterator<char>{original};
            }
        }

        // else we pass the iterator as is.
        return patchFn(input, modified);
    }

    bool Patch::Patcher::commit()
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
