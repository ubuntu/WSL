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
//

#pragma once

namespace Helpers {
	// Returns true if user system has WSLg enabled.
	bool isWslgEnabled();

	// Returns the subsystem version for this specific distro or 0 if failed.
	DWORD WslGetDistroSubsystemVersion();

	// Returns true if WSLg is enabled and distro subsystem versin is > 1.
	bool WslGraphicsSupported();

	// Runs a Linux command and checks for successfull exit.
	// Returns true if all the commands `exit 0`.
	// Launched command will not be interactive nor show any output,
	// but it blocks current thread for up to dwMilisseconds.
	bool WslLaunchSuccess(const TCHAR* command, DWORD dwMilisseconds);
}
