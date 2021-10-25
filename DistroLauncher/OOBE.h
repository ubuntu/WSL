//
//    Copyright: 2021, Canonical Ltd.
//  License: GPL-3
//  This program is free software : you can redistribute itand /or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License.
//  .
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
//  GNU General Public License for more details.
//  .
//  You should have received a copy of the GNU General Public License
//  along with this program.If not, see < https://www.gnu.org/licenses/>.
//  .
//  On Debian systems, the complete text of the GNU General
//  Public License version 3 can be found in "/usr/share/common-licenses/GPL-3".

#pragma once

namespace DistributionInfo {
    // OOBE Experience.
    ULONG OOBE();

    // isOOBEAvailable returns true if OOBE executable is found inside rootfs. 
    bool isOOBEAvailable();

    // OOBESetup executes the OOBE, creates the user and calls WslConfigureDistribution.
    HRESULT OOBESetup();

    // GetPrefillInfoInYaml generates a YAML string from Windows user and locale information.
    std::wstring GetPrefillInfoInYaml();

    // OOBE executable name.
    static TCHAR* OOBE_NAME = L"/usr/lib/libexec/wsl-setup";
}
