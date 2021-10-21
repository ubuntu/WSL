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

#include "stdafx.h"

ULONG DistributionInfo::OOBE()
{
	ULONG uid = UID_INVALID;

	// calling the oobe experience
	DWORD exitCode;
	std::wstring commandLine = L"ubuntu-wsl-oobe";
	HRESULT hr = g_wslApi.WslLaunchInteractive(commandLine.c_str(), true, &exitCode);
	if ((FAILED(hr)) || (exitCode != 0)) {
		return uid;
	}

	hr = g_wslApi.WslLaunchInteractive(L"clear", true, &exitCode);
	if (FAILED(hr)) {
		return uid;
	}

	// getting username from ouput
	// Create a pipe to read the output of the launched process.
	HANDLE readPipe;
	HANDLE writePipe;
	SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, true };
	if (CreatePipe(&readPipe, &writePipe, &sa, 0)) {
		// Query the UID of the supplied username.
		std::wstring command = L"cat /var/lib/ubuntu-wsl/assigned_account";
		int returnValue = 0;
		HANDLE child;
		HRESULT hr = g_wslApi.WslLaunch(command.c_str(), true, GetStdHandle(STD_INPUT_HANDLE), writePipe, GetStdHandle(STD_ERROR_HANDLE), &child);
		if (SUCCEEDED(hr)) {
			// Wait for the child to exit and ensure process exited successfully.
			WaitForSingleObject(child, INFINITE);
			DWORD exitCode;
			if ((GetExitCodeProcess(child, &exitCode) == false) || (exitCode != 0)) {
				hr = E_INVALIDARG;
			}

			CloseHandle(child);
			if (SUCCEEDED(hr)) {
				char buffer[64];
				DWORD bytesRead;

				// Read the output of the command from the pipe and query UID
				if (ReadFile(readPipe, buffer, (sizeof(buffer) - 1), &bytesRead, nullptr)) {
					buffer[bytesRead] = ANSI_NULL;
					try {
						const size_t bfrSize = strlen(buffer) + 1;
						wchar_t* wbuffer = new wchar_t[bfrSize];
						size_t outSize;
						mbstowcs_s(&outSize, wbuffer, bfrSize, buffer, bfrSize - 1);
						std::wstring_view uname_bfr{ wbuffer, bfrSize };
						uid = QueryUid(uname_bfr);
					}
					catch (...) {}
				}
			}
		}

		CloseHandle(readPipe);
		CloseHandle(writePipe);
	}

	return uid;
}