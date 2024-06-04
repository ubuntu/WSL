//
//    Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

#include "stdafx.h"
#include <exception>
#include <filesystem>
#include <fstream>
#include <system_error>

bool DistributionInfo::CreateUser(std::wstring_view userName)
{
    // Create the user account.
    DWORD exitCode;
    std::wstring commandLine = L"adduser --quiet --gecos '' ";
    commandLine += userName;
    HRESULT hr = g_wslApi.WslLaunchInteractive(commandLine.c_str(), true, &exitCode);
    if ((FAILED(hr)) || (exitCode != 0)) {
        return false;
    }

    // Add the user account to any relevant groups.
    commandLine = L"usermod -aG adm,dialout,cdrom,floppy,sudo,audio,dip,video,plugdev,netdev ";
    commandLine += userName;
    hr = g_wslApi.WslLaunchInteractive(commandLine.c_str(), true, &exitCode);
    if ((FAILED(hr)) || (exitCode != 0)) {

        // Delete the user if the group add command failed.
        commandLine = L"deluser ";
        commandLine += userName;
        g_wslApi.WslLaunchInteractive(commandLine.c_str(), true, &exitCode);
        return false;
    }

    return true;
}

ULONG DistributionInfo::QueryUid(std::wstring_view userName)
{
    // Create a pipe to read the output of the launched process.
    HANDLE readPipe;
    HANDLE writePipe;
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, true};
    ULONG uid = UID_INVALID;
    if (CreatePipe(&readPipe, &writePipe, &sa, 0)) {
        // Query the UID of the supplied username.
        std::wstring command = L"id -u ";
        command += userName;
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

                // Read the output of the command from the pipe and convert to a UID.
                if (ReadFile(readPipe, buffer, (sizeof(buffer) - 1), &bytesRead, nullptr)) {
                    buffer[bytesRead] = ANSI_NULL;
                    try {
                        uid = std::stoul(buffer, nullptr, 10);

                    } catch( ... ) { }
                }
            }
        }

        CloseHandle(readPipe);
        CloseHandle(writePipe);
    }

    return uid;
}

namespace {
	namespace fs = std::filesystem;
	std::pair<std::string_view, std::string_view> matchKeyValuePair(std::string_view line);
	std::wstring str2wide(const std::string& str, UINT codePage = CP_THREAD_ACP);

	class IniReader {
	public:
		explicit IniReader(fs::path path) : file{ path } {}
		// Returns the value of [section].key as a string.
		// To ease distinguishing the parameters, section is expected to contain the square-brackets.
		std::string operator()(std::string_view section, std::string_view key) {
			if (!seekSection(section)) {
				return {};
			}
			return findValueInCurrentSection(key);
		}
		~IniReader() = default;
	private:
		// Positions the file stream past the line where section is declared.
		// Returns false if section is not found.
		// Note: this function always starts from the beginning of the file.
		bool seekSection(std::string_view section) {
			std::string line;
			file.seekg(0);
			while (std::getline(file, line) && line.find(section, 0) == std::string::npos);
			return !file.eof();
		}

		// Iterates file until key, another section or EOF is found.
		// If the key is found, its value is returned. Empty string, otherwise.
		std::string findValueInCurrentSection(std::string_view key) {
			for (std::string line; std::getline(file, line);) {
				if (line.find('[', 0) != std::string::npos) {
					// found another section, so value not found.
					return {};
				}
				if (auto [k, v] = matchKeyValuePair(line); k == key) {
					return std::string{ v };
					}
				}

				return {};
			}
		
		std::ifstream file;
	};

	ULONG defaultUserInWslConf() try {
		fs::path etcWslConf{ L"\\\\wsl.localhost" };
		etcWslConf /= DistributionInfo::Name;
		etcWslConf /= "etc\\wsl.conf";

		std::error_code err;
		if (!fs::exists(etcWslConf, err)) {
			_putws(L"CheckInitTasks: /etc/wsl.conf does not exist");
			return 0;
		}
		if (err) {
			Helpers::PrintErrorMessage(HRESULT_FROM_WIN32(err.value()));
			return 0;
		}

		IniReader ini{ etcWslConf };
		std::string defaultUser = ini("[user]", "default");
		if (defaultUser.empty()) {
			_putws(L"CheckInitTasks: default user not found in /etc/wsl.conf.");
			return 0;
		}

		return DistributionInfo::QueryUid(str2wide(defaultUser));
		}
	catch (std::exception const&) {
		_putws(L"CheckInitTasks: failed to read /etc/wsl.conf");
		return {};
	}

	// Returns the key-value pair if line contains one, or empty string_views otherwise.
	std::pair<std::string_view, std::string_view> matchKeyValuePair(std::string_view line) {
		static constexpr char whitespaces[] = " \n\r\f\v\t";
		std::size_t keyStart = line.find_first_not_of(whitespaces, 0);
		if (keyStart == std::string::npos) {
			return {}; // empty/blank line.
		}
		std::size_t equalSignPos = line.find_first_of('=', keyStart);
		if (equalSignPos == std::string::npos) {
			return {}; // not a key-value pair.
		}
		std::size_t keyLength = equalSignPos - keyStart;
		if (keyLength <= 0) {
			return {}; // line starts with '='?
		}
		// first non whitespace char after the '='.
		std::size_t valueStarts = line.find_first_not_of(whitespaces, equalSignPos + 1);
		if (valueStarts == std::string::npos) {
			return {}; // no value.
		}
		// first whitespace after the value start position.
		std::size_t valueEnds = line.find_first_of(whitespaces, valueStarts);
		if (valueEnds == std::string::npos) {
			// there is no whitespace, not even a new line.
			valueEnds = line.length();
		}

		std::size_t valueLength = valueEnds - valueStarts;
		std::string_view key = line.substr(keyStart, keyLength);
		std::string_view value = line.substr(valueStarts, valueLength);
		return { key,value };
	}

	std::wstring str2wide(const std::string& str, UINT codePage)
	{
		if (str.empty() || str.size() >= INT_MAX) return {};

		int inputSize = static_cast<int>(str.size());
		int required = ::MultiByteToWideChar(codePage, 0, str.data(), inputSize, NULL, 0);
		if (0 == required) return {};

		std::wstring str2;
		str2.resize(required);

		int converted = ::MultiByteToWideChar(codePage, 0, str.data(), inputSize, &str2[0], required);
		if (0 == converted) return {};

		return str2;
	}
}

bool DistributionInfo::CheckInitTasks(WslApiLoader& api, bool wantUser)
{
	_putws(L"Checking for initialization tasks...");

	DWORD exitCode = -1;
	if (auto hr = api.WslLaunchInteractive(L"which -s cloud-init", FALSE, &exitCode); FAILED(hr)) {
		Helpers::PrintErrorMessage(hr);
		return false;
	}
	if (exitCode != 0) {
		_putws(L"Distro version doesn't support initialization tasks.\n");
		return false;
	}

	exitCode = -1;
	if (auto hr = api.WslLaunchInteractive(L"cloud-init status --wait", FALSE, &exitCode); FAILED(hr)) {
		Helpers::PrintErrorMessage(hr);
		return false;
	}
	// 0 for "success" or 2 for "recoverable error" - sometimes we get that while status shows "done".
	// https://cloudinit.readthedocs.io/en/latest/explanation/failure_states.html#cloud-init-error-codes
	// If that's the case we should inspect the system to check whether we should skip or proceed with user creation.
	if (exitCode != 0 && exitCode != 2) {
		wprintf(L"Distro initialization failed with exit code: %u\n", exitCode);
		return false;
	}

	if (!wantUser) {
		return true;
	}

	auto uid = defaultUserInWslConf();
	if (uid == 0) {
		return false;
	}

	// Use the same flags as the upstream `CreateUser()` function.
	if (auto hr = api.WslConfigureDistribution(uid, WSL_DISTRIBUTION_FLAGS_DEFAULT);
		FAILED(hr)) {
		Helpers::PrintErrorMessage(hr);
		return false;
	}

	return true;
}
