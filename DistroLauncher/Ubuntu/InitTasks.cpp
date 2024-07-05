#include <stdafx.h>
#include "InitTasks.h"

#include <algorithm>
#include <charconv>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <optional>
#include <vector>
#include <system_error>

namespace Ubuntu {

namespace {
// Blocks the current thread until all initialization tasks finish.
void waitForInitTasks(WslApiLoader& api);

// Enforces the existence of a default WSL user either:
// - defined in /etc/wsl.conf (which might not be in effect yet)
// - defined in WSL API/registry
// - or the lowest UID >= 1000 in the NSS passwd database
// Returns false if a default user couldn't be set.
bool enforceDefaultUser(WslApiLoader& api);
}  // namespace

bool CheckInitTasks(WslApiLoader& api, bool checkDefaultUser) {
  waitForInitTasks(api);

  if (!checkDefaultUser) {
    return true;
  }

  return enforceDefaultUser(api);
}

namespace {
void waitForInitTasks(WslApiLoader& api) {
  _putws(L"Checking for initialization tasks...\n");

  DWORD exitCode_ = -1;
  if (auto hr = api.WslLaunchInteractive(L"which -s cloud-init", FALSE, &exitCode_); FAILED(hr)) {
    Helpers::PrintErrorMessage(hr);
    return;
  }
  if (exitCode_ != 0) {
    _putws(L"CheckInitTasks: this release doesn't support initialization tasks.\n");
    return;
  }

  exitCode_ = -1;
  if (auto hr = api.WslLaunchInteractive(L"cloud-init status --wait", FALSE, &exitCode_);
      FAILED(hr)) {
    Helpers::PrintErrorMessage(hr);
    return;
  }
  // 0 for "success" or 2 for "recoverable error" - sometimes we get that while status shows "done".
  // https://cloudinit.readthedocs.io/en/latest/explanation/failure_states.html#cloud-init-error-codes
  // If that's the case we should inspect the system to check whether we should skip or proceed with
  // user creation.
  if (exitCode_ != 0 && exitCode_ != 2) {
    wprintf(L"CheckInitTasks: failed with exit code: %u\n", exitCode_);
  }
}

// Groups the pieces of information from a single user entry in the passwd database we care about.
struct UserEntry {
  std::string name;
  ULONG uid = -1;
};

namespace fs = std::filesystem;
fs::path wslConfPath() {
  // init-once, lazily.
  static fs::path etcWslConf =
      fs::path{L"\\\\wsl.localhost"} / DistributionInfo::Name / "etc\\wsl.conf";
  return etcWslConf;
}

void writeUserToWslConf(std::string_view name) {
  auto oldPath = wslConfPath();
  auto newPath = wslConfPath();
  newPath.replace_extension(".new");

  {
    // Write the new file starting with the [user] section.
    // ios::binary is needed otherwise '\r' will be added on Windows :(.
    std::ofstream confNew{newPath, std::ios::binary};
    confNew << "[user]\ndefault=" << name << '\n';

    // Read existing content.
    std::ifstream confOld{oldPath};
    std::string original{std::istreambuf_iterator<char>(confOld), std::istreambuf_iterator<char>()};
    if (!original.empty()) {
      // and copy that to the new file.
      std::copy(original.begin(), original.end(), std::ostreambuf_iterator<char>(confNew));
    }
  }

  // Files are closed by now, we may replace the old file with the new one.
  fs::rename(newPath, oldPath);
}

bool setDefaultUserViaWslApi(WslApiLoader& api, unsigned long uid) {
  if (auto hr = api.WslConfigureDistribution(uid, WSL_DISTRIBUTION_FLAGS_DEFAULT); FAILED(hr)) {
    Helpers::PrintErrorMessage(hr);
    return false;
  }
  return true;
}

// Collects all users found in the NSS passwd database, sorted by UID.
std::vector<UserEntry> getAllUsers(WslApiLoader& api);

// Returns the defaultUser set in /etc/wsl.conf or the empty string if none is set.
std::string defaultUserInWslConf();

// Converts a multi-byte null-terminated string into a wide string.
std::wstring str2wide(std::string_view str, UINT codePage = CP_THREAD_ACP);

bool enforceDefaultUser(WslApiLoader& api) try {
  auto users = getAllUsers(api);

  if (users.empty()) {
    // unexpectedly nothing to do
    _putws(L"CheckInitTasks: couldn't find any users in NSS database\n");
    return false;
  }
  // 1. We read the default user name from /etc/wsl
  if (auto name = defaultUserInWslConf(); !name.empty()) {
    auto found = std::find_if(users.begin(), users.end(),
                              [&name](const UserEntry& u) { return u.name == name; });
    if (found != users.end()) {
      return setDefaultUserViaWslApi(api, found->uid);
    }
    wprintf(
        L"CheckInitTasks: user \"%s\" referred in /etc/wsl.conf "
        L"was not found in the system, entry may be overwritten.\n",
        str2wide(name).c_str());
  }
  // 2. Check for the Windows registry
  // This call returns the UID of the current default user (most likely root, unless someone set a
  // different UID via the registry editor or WSL API).
  if (auto uid = DistributionInfo::QueryUid(L""); uid != 0) {
    auto found = std::find_if(users.begin(), users.end(),
                              [&uid](const UserEntry& u) { return u.uid == uid; });
    if (found != users.end()) {
      writeUserToWslConf(found->name);
      return true;
    }
  }

  // 3. Finally, search for the first non-system user.
  // UID 65534 is typically the user 'nobody' and non-system users have UID>=1000.
  auto found = std::find_if(users.begin(), users.end(),
                            [](const UserEntry& u) { return u.uid > 999 && u.uid < 65534; });
  if (found != users.end()) {
    writeUserToWslConf(found->name);
    // Do this last because its effect is immediate, i.e. the resulting default user could not be
    // allowed to write /etc/wsl.conf for example.
    return setDefaultUserViaWslApi(api, found->uid);
  }

  _putws(L"CheckInitTasks: no candidate default user was found\n");
  return false;
} catch (...) {
  _putws(L"CheckInitTasks: Unexpected failure when trying to set the default WSL user");
  return false;
}

// A truly temporary file copy.
//
// Invariant: while this object exists, so does the underlying file,
// being auto-deleted by the OS when this object goes out of scope.
class TempFileCopy {
  fs::path p;
  HANDLE h = nullptr;

 public:
  ~TempFileCopy() {
    if (h) {
      CloseHandle(h);
    }
  }
  const fs::path& path() const { return p; }

  // Creates a temporary copy of the source file configured for auto removal by the OS when this
  // object goes out of scope under the system's preferred temporary directory.
  explicit TempFileCopy(const fs::path& source) {
    // When debugging fs::temp_directory_path() returns a path like LOCALAPPDATA\Temp, but once
    // packaged it should point inside LOCALAPPDATA\Packages\<PackageFullName>\...
    auto path = fs::temp_directory_path();

    wchar_t rawDestination[MAX_PATH] = {'\0'};
    if (auto uniqueCode = GetTempFileNameW(path.native().c_str(), L"wsl", 0, rawDestination);
        uniqueCode == 0) {
      std::string msg{"couldn't create a temporary file inside "};
      msg += path.string();
      throw std::system_error{static_cast<int>(GetLastError()), std::system_category(), msg};
    }
    fs::path destination{rawDestination};

    // GetTempFileNameA would have created a file for us thus we need to overwrite it.
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
    fs::permissions(destination, fs::perms::owner_read | fs::perms::owner_write);

    // Allow others to read but not write it, and configure it to be deleted automatically when the
    // handle is closed (what's the destructor does automatically no matter what).
    HANDLE file = CreateFileW(
        rawDestination, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_READONLY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
      std::string msg{"couldn't open file in auto-delete mode: "};
      msg += path.string();
      throw std::system_error{static_cast<int>(GetLastError()), std::system_category(), msg};
    }

    // fully initializes the object.
    p = destination;
    h = file;
  }
};

// Reads [user].default from an existing wsl.conf file located at the ini path provided.
std::string readIniDefaultUser(const fs::path& ini) {
  // `getconf LOGIN_NAME_MAX` returns 256, but glibc restricts user login names to 32 chars long.
  static constexpr auto utNameSize = 33;  // With room for the NULL terminator just in case.
  char uname[utNameSize] = {'\0'};
  auto len =
      GetPrivateProfileStringA("user", "default", nullptr, uname, utNameSize, ini.string().c_str());

  // GetPrivateProfileStringA set errno to ERROR_FILE_NOT_FOUND if the file
  // or section or key are not found or if the file is ill-formed. We know the file exists.
  // Absent user.default entry is a common case, we don't want to spam users for that.
  // I don't see what other errors that could raise.
  if (auto e = GetLastError(); len == 0 && e == ERROR_FILE_NOT_FOUND) {
    return {};
  }

  return uname;
}

std::string defaultUserInWslConf() try {
  auto etcWslConf = wslConfPath();
  if (!fs::exists(etcWslConf)) {
    return {};
  }
  // Copy /etc/wsl.conf to a temporary local path (guaranteed by the OS to be auto-deleted) from
  // where GetPrivateProfileStringA can read it.
  TempFileCopy copy(etcWslConf);
  return readIniDefaultUser(copy.path());

} catch (std::system_error const& err) {
  // std::filesystem_error is child of std::system_error
  std::wcout << L"CheckInitTasks: failed to read /etc/wsl.conf: " << err.code() << ": "
             << str2wide(err.what());
  return {};
}

std::wstring str2wide(std::string_view str, UINT codePage) {
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

// A non-interactive WSL process, turned into a class so we don't have to worry about closing
// the process and pipe's handles.
class WslProcess {
 private:
  HANDLE process_ = nullptr;
  HANDLE readPipe_ = nullptr;
  HANDLE writePipe_ = nullptr;
  std::wstring command_;

  static constexpr auto MaxOutputSize = 4096;

 public:
  ~WslProcess() {
    if (process_) {
      CloseHandle(process_);
    }
    if (writePipe_) {
      CloseHandle(writePipe_);
    }
    if (readPipe_) {
      CloseHandle(readPipe_);
    }
  }

  struct Result {
    std::wstring error;
    std::size_t exitCode = static_cast<std::size_t>(-1);
    std::string stdOut;
  };

  // Runs the process via WSL api and wait for timeout milliseconds.
  Result run(WslApiLoader& api, DWORD timeout);

  explicit WslProcess(std::wstring command_) : command_{command_} {};
};

std::vector<UserEntry> getAllUsers(WslApiLoader& api) {
  WslProcess getent{L"getent passwd"};
  auto [error, exitCode, output] = getent.run(api, 10'000);
  if (!error.empty()) {
    _putws(L"failed to read passwd database: ");
    _putws(error.c_str());
    if (exitCode != 0) {
      wprintf(L"%lld", exitCode);
    }
    return {};
  }

  if (output.empty()) {
    return {};
  }

  // Partially parsing getent output, breaking on the first ill-formed line.
  // We only care about login name and UID:
  // ^NAME:ENCRYPTION:UID:...\n$
  std::vector<UserEntry> users;
  std::string line;
  for (std::istringstream pipeReader{output}; std::getline(pipeReader, line);) {
    std::istringstream lineReader{line};
    std::string name, unused, u;
    if (!std::getline(lineReader, name, ':')) {
      break;
    }
    if (!std::getline(lineReader, unused, ':')) {
      break;
    }
    if (!std::getline(lineReader, u, ':')) {
      break;
    }

    ULONG uid = -1;
    auto ud = std::from_chars(u.data(), u.data() + u.length(), uid);
    if (ud.ec != std::errc{}) {
      break;
    }
    users.push_back(UserEntry{name, uid});
  }

  std::sort(users.begin(), users.end(),
            [](const UserEntry& a, const UserEntry& b) { return a.uid < b.uid; });
  return users;
}

WslProcess::Result WslProcess::run(WslApiLoader& api, DWORD timeout) {
  // Create a pipe to read the output of the launched process.
  HANDLE read, write, process;
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, true};
  ULONG uid = -1;
  if (CreatePipe(&read, &write, &sa, 0) == FALSE) {
    return {L"failed to create the stdio pipe"};
  }
  // We have to remember to close the pipe handles.
  readPipe_ = read;
  writePipe_ = write;

  DWORD exitCode = -1;
  auto hr = api.WslLaunch(command_.c_str(), FALSE, GetStdHandle(STD_INPUT_HANDLE), writePipe_,
                          GetStdHandle(STD_ERROR_HANDLE), &process);
  if (FAILED(hr)) {
    return {L"failed to launch process"};
  }
  // Also need to remember to close the process handle.
  process_ = process;

  if (auto wait = WaitForSingleObject(process_, timeout); wait == WAIT_TIMEOUT) {
    return {L"terminated due timed out"};
  }

  exitCode = -1;
  if ((GetExitCodeProcess(process_, &exitCode) == false) || (exitCode != 0)) {
    return {L"exited with error", exitCode};
  }

  // Check how many bytes we need to allocate to pump all contents out the pipe.
  DWORD unreadBytes;
  if (FALSE == PeekNamedPipe(readPipe_, 0, 0, 0, &unreadBytes, 0) || unreadBytes == 0) {
    return {L"could not read the process output", 0};
  }
  std::size_t size = 1ULL + unreadBytes;
  if (size > MaxOutputSize) {
    return {L"process output is too big", 0};
  }
  std::string contents(size, '\0');

  // Finally we do the read.
  DWORD readCount = 0, avail = 0;
  if (FALSE == ReadFile(readPipe_, contents.data(), unreadBytes, &readCount, nullptr) ||
      read == 0) {
    return {L"could not read the process output", 0};
  }

  return {{}, 0, contents};
}

}  // namespace
}  // namespace Ubuntu
