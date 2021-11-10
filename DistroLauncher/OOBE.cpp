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

namespace DistributionInfo {

	namespace {
		std::wstring PreparePrefillInfo();
	}

	bool DistributionInfo::isOOBEAvailable(){
		DWORD exitCode=-1;
		std::wstring whichCdm{ L"which " };
		whichCdm.append(DistributionInfo::OOBE_NAME);
		HRESULT hr = g_wslApi.WslLaunchInteractive(whichCdm.c_str(), true, &exitCode);
		// true if launching the process succeeds and `which` command returns 0.
		return ((SUCCEEDED(hr)) && (exitCode == 0));
	}

	HRESULT DistributionInfo::OOBESetup()
	{
		// comfigre the distribution before calling OOBE
		HRESULT hr = g_wslApi.WslConfigureDistribution(0, WSL_DISTRIBUTION_FLAGS_DEFAULT);
		if (FAILED(hr)) {
			return hr;
		}

		// calling the oobe experience
		DWORD exitCode;
		// Prepare prefill information to send to the OOBE.
		std::wstring prefillCLIPostFix = DistributionInfo::PreparePrefillInfo();
		std::wstring commandLine = DistributionInfo::OOBE_NAME + prefillCLIPostFix;
		hr = g_wslApi.WslLaunchInteractive(commandLine.c_str(), true, &exitCode);
		if ((FAILED(hr)) || (exitCode != 0)) {
			return hr;
		}

		hr = g_wslApi.WslLaunchInteractive(L"clear", true, &exitCode);
		if (FAILED(hr)) {
			return hr;
		}

		// read the final statis
		HANDLE readPipe;
		HANDLE writePipe;
		SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, true };
		if (CreatePipe(&readPipe, &writePipe, &sa, 0)) {
			// check the content of the launcher status.
			std::wstring command = L"cat /run/subiquity/launcher-status";
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

					// Read the output of the command from the pipe and read the status
					if (ReadFile(readPipe, buffer, (sizeof(buffer) - 1), &bytesRead, nullptr)) {
						buffer[bytesRead] = ANSI_NULL;
						try {
							const size_t bfrSize = strlen(buffer) + 1;
							wchar_t* wbuffer = new wchar_t[bfrSize];
							size_t outSize;
							mbstowcs_s(&outSize, wbuffer, bfrSize, buffer, bfrSize - 1);
							std::wstring_view status_bfr{ wbuffer, bfrSize };
							hr = OOBEStatusHandling(status_bfr);
						}
						catch (...) {}
					}
				}
			}

			CloseHandle(readPipe);
			CloseHandle(writePipe);
		}

		return hr;
	}

	HRESULT DistributionInfo::OOBEStatusHandling(std::wstring_view status) {
		// Check the status passed from Linux side and perform the corresponding actions
		std::wstring statusl{ status };
		bool is_reboot = false;

		if (statusl.compare(L"complete") == 0) {
			// do nothing; just return
			return S_OK;
		}
		else if (statusl.compare(L"reboot") == 0) {
			is_reboot = true;
		}
		else if (statusl.compare(L"shutdown") != 0) {
			// unaccepted status
			return E_NOT_VALID_STATE;
		}

		std::wstring ARG_SET = L"-t " + DistributionInfo::Name;

		SHELLEXECUTEINFO ShExecInfo;
		ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
		ShExecInfo.fMask = NULL;
		ShExecInfo.hwnd = 0;
		ShExecInfo.lpVerb = L"open";
		ShExecInfo.lpFile = L"C:\\Windows\\Sysnative\\wsl.exe";
		ShExecInfo.lpParameters = ARG_SET.c_str();
		ShExecInfo.lpDirectory = NULL;
		ShExecInfo.nShow = SW_HIDE;
		ShExecInfo.hInstApp = NULL;

		bool Result = ShellExecuteEx(&ShExecInfo);

		if (!Result) {
			return ERROR_FAIL_SHUTDOWN;
		}


		if (is_reboot) {
			//constructing the current windows executable location
			TCHAR szPath[MAX_PATH];
			if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath)))
			{
				PathAppend(szPath, _T("\\Microsoft\\WindowsApps\\"));
				PathAppend(szPath, DistributionInfo::WINEXEC_NAME);
			}

			ShExecInfo.lpFile = szPath;
			ShExecInfo.lpParameters = NULL;
			ShExecInfo.nShow = SW_SHOWNORMAL;

			Result = ShellExecuteEx(&ShExecInfo);
			if (!Result) {
				return ERROR_FAIL_RESTART;
			}
		}

		return S_OK;

	}

	// Anonimous namespace to avoid exposing internal details of the implementation.
	namespace {

		// Saves Windows information inside Linux filesystem to supply to the OOBE.
		// Returns empty string if we fail to generate the prefill file
		// or the postfix to be added to the OOBE command line.
		std::wstring PreparePrefillInfo() {
			std::wstring commandLine;
			std::wstring prefillInfo = DistributionInfo::GetPrefillInfoInYaml();
			if (prefillInfo.empty()) {
				return commandLine;
			}

			// Write it to a file inside \\wsl$ distro filesystem.        
			const std::wstring prefillFileNameDest = L"/var/tmp/prefill-system-setup.yaml";
			const std::wstring wslPrefix = L"\\\\wsl$\\" + DistributionInfo::Name;
			std::wofstream prefillFile;
			// Mixing slashes and backslashes that way is not a problem for Windows.
			prefillFile.open(wslPrefix + prefillFileNameDest, std::ios::ate);
			if (prefillFile.fail()) {
				Helpers::PrintErrorMessage(CO_E_FAILEDTOCREATEFILE);
				return commandLine;
			}

			prefillFile << prefillInfo;
			prefillFile.close();
			if (prefillFile.fail()) {
				Helpers::PrintErrorMessage(CO_E_FAILEDTOCREATEFILE);
				return commandLine;
			}
			
			commandLine += L" --prefill=" + prefillFileNameDest;
			return commandLine;
		} // std::wstring PreparePrefillInfo().

	} // namespace.

} // namespace DistributionInfo.
