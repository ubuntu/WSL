#pragma once

namespace Oobe
{
    struct InstallerPolicy
    {
        static const wchar_t* const OobeCommand;
        static constexpr auto crashedExitCode = -5;

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
            std::wstring wslPrefixedDest = WslPathPrefix() + DistributionInfo::Name;
            wslPrefixedDest.append(to);
            std::filesystem::path realDest{wslPrefixedDest};
            std::error_code ec;
            return std::filesystem::copy_file(from, realDest, ec);
        }

        /// Attempts to find and hide the installer GUI window.
        static HWND try_hiding_installer_window(int repeatTimes)
        {
            // attempt to find the window by class "RAIL_WINDOW" and flutterCaption "Ubuntu WSL (UbuntuPreview)".
            // it appears before Subiquity is ready.
            constexpr auto fastSleepFor = 10;
            HWND rdpWindow = nullptr;
            std::array<std::wstring, 2> possibleCaptions{L"Ubuntu WSL (", L"[WARN:COPY MODE] Ubuntu WSL ("};
            std::for_each(possibleCaptions.begin(), possibleCaptions.end(), [](auto& caption) {
                caption.append(DistributionInfo::Name);
                caption.append(1, ')');
            });

            for (int i = 0; i < repeatTimes; ++i) {

                for (const auto& caption : possibleCaptions) {
                    HWND res = FindWindow(L"RAIL_WINDOW", caption.c_str());
                    if (res == nullptr) {
                        continue;
                    }

                    rdpWindow = res;
                    ShowWindow(rdpWindow, SW_HIDE);
                    return rdpWindow;
                };
                Sleep(fastSleepFor);
            }
            return rdpWindow;
        }

        static void show_window(HWND window)
        {
            // Currently when the window is moved programmatically its buttons and interactive elements appear
            // mis calibrated in the screen, i.e. user needs to find some offset between the position of the interactive
            // element and the clickable area.

            // Win32Utils::center_window(window);
            ShowWindow(window, SW_RESTORE);
        }
        // Repeatedly launches asynchronously the [command] up to [repeatTimes] until it succeeds.
        // Return false if attempts are exhausted with no success. This is suitable for launching small processes that
        // can be used to monitor the status of long running processes.
        // If the monitored process is never ready, this function will clean it up.
        static bool poll_success(const wchar_t* command, int repeatTimes, HANDLE monitoredProcess)
        {
            using defer = std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&::CloseHandle)>;
            constexpr DWORD watcherTimeout = 1000; // ms
            constexpr auto initialDelay = 3e3F;
            constexpr auto delayRatio = 0.65F;

            HANDLE sslxProcess;
            DWORD sslxExitCode;
            HRESULT sslxHr;

            auto delaysGenerator = [cur = initialDelay, q = delayRatio]() mutable {
                cur *= q;
                return cur;
            };

            for (int i = 0; i < repeatTimes; ++i) {
                sslxProcess = nullptr;
                sslxExitCode = -1;

                // We most likely will not want to see any output of this command, just its exit code.
                sslxHr = g_wslApi.WslLaunch(command, TRUE, nullptr, nullptr, nullptr, &sslxProcess);

                if (SUCCEEDED(sslxHr)) {
                    defer guard{sslxProcess, CloseHandle};

                    // Notice that WslLaunch is asynchronous, so we can await on it for some controlled amount of time.
                    if (WaitForSingleObject(sslxProcess, watcherTimeout) != WAIT_OBJECT_0) {

                        // If the process didn't exit within the timeout we kill it.
                        TerminateProcess(sslxProcess, crashedExitCode);
                    }
                    auto getExit = GetExitCodeProcess(sslxProcess, &sslxExitCode);
                    if (getExit != FALSE && sslxExitCode == 0) {
                        return true;
                    }
                }

                // Sleeps progressively less time than the last iteration.
                Sleep(static_cast<DWORD>(delaysGenerator()));
            }

            // started but is never ready, so we must kill it.
            TerminateProcess(monitoredProcess, crashedExitCode);
            CloseHandle(monitoredProcess);
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
                exitCode = crashedExitCode;
                TerminateProcess(process, exitCode);
                return exitCode;
            }

            GetExitCodeProcess(process, &exitCode);
            // The Flutter installer exits 0 on some crashes.
            // And sometimes reports crash even though Subiquity has finished. This gets fixed in [UDI PR
            // 786](https://github.com/canonical/ubuntu-desktop-installer/pull/786)
            if (exitCode == 0) {
                g_wslApi.WslLaunchInteractive(L"grep -E 'EXITED|DONE' /run/subiquity/server-state", FALSE, &exitCode);
                DWORD clearExitCode;
                g_wslApi.WslLaunchInteractive(L"clear", FALSE, &clearExitCode);
            }
            return exitCode;
        }

        /// Launches the OOBE installer [command] asynchronously and return its handle if launched successfully without
        /// waiting for its completion. It's caller responsibility to monitor and clean-up the returned process handle.
        static HANDLE start_installer_async(const wchar_t* command)
        {
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

            return child;
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
    inline const wchar_t* const InstallerPolicy::OobeCommand = L"sudo /usr/libexec/wsl-setup";
}
