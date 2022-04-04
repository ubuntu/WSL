#pragma once
#include <type_traits>
#include <iterator>
#include <utility>
#include <filesystem>
namespace Oobe
{
    struct InstallerPolicy
    {
        static const wchar_t* OobeCommand;

        static bool is_oobe_available()
        {
            return DistributionInfo::isOOBEAvailable();
        }

        static std::wstring prepare_prefill_info()
        {
            return DistributionInfo::PreparePrefillInfo();
        }

        // Returns true if the OOBE must be launched in text mode.
        static bool must_run_in_text_mode()
        {
            return DistributionInfo::mustRunOOBEinTextMode();
        }

        // Detect and act upon the launcher command file left by the OOBE.
        static void handle_exit_status()
        {
            ExitStatusHandling();
        }

        static bool copy_file_into_distro(const std::filesystem::path& from, const std::wstring& to)
        {
            std::wstring wslPrefixedDest = L"\\\\wsl.localhost\\" + DistributionInfo::Name;
            wslPrefixedDest.append(to);
            std::filesystem::path realDest{wslPrefixedDest};
            std::error_code ec;
            return std::filesystem::copy_file(from, realDest, ec);
        }

        // Repeatedly launches asynchronously the [command] up to [repeatTimes] until it succeeds.
        // Return false if attempts are exhausted with no success. This is suitable for launching small processes that
        // can be used to monitor the status of long running processes.
        static bool poll_success(const wchar_t* command, int repeatTimes)
        {
            using defer = std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&::CloseHandle)>;
            constexpr DWORD watcherTimeout = 1000; // ms

            HANDLE sslxProcess;
            DWORD sslxExitCode;
            HRESULT sslxHr;

            auto delaysGenerator = [cur = 11e3f, q = 0.65f]() mutable {
                cur *= q;
                return cur;
            };

            for (int i = 0; i < repeatTimes; ++i) {
                sslxProcess = nullptr;
                sslxExitCode = -1;
                sslxHr = E_INVALIDARG;

                sslxHr =
                  g_wslApi.WslLaunch(command, TRUE, GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE),
                                     GetStdHandle(STD_ERROR_HANDLE), &sslxProcess);

                if (SUCCEEDED(sslxHr)) {
                    defer guard{sslxProcess, CloseHandle};

                    // Notice that WslLaunch is asynchronous, so we can await on it for some controlled amount of time.
                    if (WaitForSingleObject(sslxProcess, watcherTimeout) != WAIT_OBJECT_0) {

                        // If the process didn't exit within the timeout we kill it.
                        TerminateProcess(sslxProcess, -5);
                    }
                    auto getExit = GetExitCodeProcess(sslxProcess, &sslxExitCode);
                    if (getExit != FALSE && sslxExitCode == 0) {
                        return true;
                    }
                }

                // Sleeps progressively less time than the last iteration.
                Sleep(static_cast<DWORD>(delaysGenerator()));
            }
            return false;
        }

        // Blocks the calling thread until the [process] exits or [timeout] expires.
        // This function closes the [process] handle in the end.
        // Returns the [process] exit code.
        static DWORD consume_process(HANDLE process, DWORD timeout)
        {
            using defer = std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&::CloseHandle)>;
            defer processGuard{process, CloseHandle};

            DWORD exitCode = -1;
            if (WaitForSingleObject(process, timeout) != WAIT_OBJECT_0) {
                exitCode = -5;
                TerminateProcess(process, exitCode);
                return exitCode;
            }

            GetExitCodeProcess(process, &exitCode);
            return exitCode;
        }

        // Launches the OOBE installer [command] asynchronously and block this thread until it's ready for
        // interaction, according to the prescription of the [watcher] command. If this succeeds, caller is responsible
        // for the lifetime of the process represented by the returned handle.
        static HANDLE start_installer_async(const wchar_t* command,
                                            const wchar_t* watcher = L"ss -lx | grep subiquity &>/dev/null")
        {
            constexpr int numIterations = 8;
            DWORD exitCode = -1;
            HANDLE child = nullptr;
            HRESULT hr = g_wslApi.WslLaunch(command, TRUE, GetStdHandle(STD_INPUT_HANDLE),
                                            GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE), &child);
            if (FAILED(hr)) {
                if (child != nullptr) {
                    // Launching the command failed but some handle was created, thus it must be closed.
                    CloseHandle(child);
                }
                return nullptr;
            }

            if (poll_success(watcher, numIterations)) {
                // Ownership of the installer sslxProcess handle is being passed by.
                return child;
            }
            // started but is never ready, so we must kill it.
            TerminateProcess(child, -5);
            CloseHandle(child);
            return nullptr;
        }

        // Launches the OOBE command by [cli] synchronously, blocking this thread from start to finish.
        // Returns its exit code.
        static DWORD do_launch_sync(const wchar_t* cli)
        {
            DWORD exitCode = -1;
            HRESULT hr = g_wslApi.WslLaunchInteractive(cli, FALSE, &exitCode);
            // I'm not sure if it's possible to have exitCode 0 and a failed HRESLT from WSL API. But just to make sure:
            if ((FAILED(hr)) && (exitCode == 0)) {
                return -1;
            }
            return exitCode;
        }
    };

    // This imposes deprecation of the symbol DistributionInfo::OOBE_NAME exposed in OOBE.h.
    // This is useful because the controller can launch the OOBE in a couple of different ways, but in all of them we
    // need `sudo`. At some point those files OOBE.cpp and OOBE.h will be dropped in favor of this one.
    inline const wchar_t* InstallerPolicy::OobeCommand = L"sudo /usr/libexec/wsl-setup";
}
