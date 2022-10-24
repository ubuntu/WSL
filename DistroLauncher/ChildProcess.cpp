#include "stdafx.h"

namespace Oobe
{

    void Win32ChildProcess::onClose(void* data, BOOLEAN /*unused*/)
    {
        auto* instance = static_cast<Win32ChildProcess*>(data);
        instance->unsubscribe();
        // Process ended anyway.
        instance->destroy();
        instance->notifyListener();
    }

    void Win32ChildProcess::do_unsubscribe()
    {
        if (waiterHandle != nullptr) {
            UnregisterWait(waiterHandle);
        }
    }

    void Win32ChildProcess::destroy()
    {
        CloseHandle(procInfo.hThread);
        CloseHandle(procInfo.hProcess);
        procInfo.hProcess = nullptr;
    }

    void Win32ChildProcess::do_terminate()
    {
        // NOLINTNEXTLINE(performance-no-int-to-ptr) - that's the Win32 way.
        if (procInfo.hProcess != nullptr && procInfo.hProcess != INVALID_HANDLE_VALUE) {
            TerminateProcess(procInfo.hProcess, 0);
            destroy();
        }
    }

    bool Win32ChildProcess::do_start()
    {
        if (!std::filesystem::exists(executable)) {
            std::wcerr << L"Executable <" << executable << L"> doesn't exist.\n";
            return false;
        }

        auto cli = executable.wstring();
        if (!arguments.empty()) {
            cli.append(1, L' ');
            cli.append(arguments);
        }

        const DWORD flags = appConfig().requiresNewConsole ? CREATE_NEW_CONSOLE : 0;

        BOOL res = CreateProcess(nullptr,                   // command line
                                 cli.data(),                // non-const CLI
                                 &sa,                       // process security attributes
                                 nullptr,                   // primary thread security attributes
                                 TRUE,                      // handles are inherited
                                 flags,                     // creation flags
                                 nullptr,                   // use parent's environment
                                 nullptr,                   // use parent's current directory
                                 &startInfo,                // STARTUPINFO pointer
                                 &procInfo);                // output: PROCESS_INFORMATION
        auto ok = res != 0 && procInfo.hProcess != nullptr; // success
        if (ok) {
            if (appConfig().requiresNewConsole) {
                // This doesn't seem useful if the child is a window application. But showed useful when dealing with
                // the PoC e2e test. See https://github.com/ubuntu/WSL/pull/278
                WaitForInputIdle(procInfo.hProcess, INFINITE);
            }
            RegisterWaitForSingleObject(&waiterHandle, procInfo.hProcess, onClose, this, INFINITE,
                                        WT_EXECUTEDEFAULT | WT_EXECUTEONLYONCE);
        }

        DWORD exitCode = 0;
        return ok && TRUE == GetExitCodeProcess(procInfo.hProcess, &exitCode) && exitCode == STILL_ACTIVE; // success;
    }

    DWORD Win32ChildProcess::do_waitExitSync(DWORD timeout)
    {
        if (auto waitRes = WaitForSingleObject(procInfo.hProcess, timeout); waitRes != WAIT_OBJECT_0) {
            return waitRes;
        }
        DWORD exitCode = 0;
        GetExitCodeProcess(procInfo.hProcess, &exitCode);
        return exitCode;
    }
}
