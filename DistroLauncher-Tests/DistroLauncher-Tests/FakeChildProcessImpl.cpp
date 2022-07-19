#include "stdafx.h"
#include "ChildProcess.h"

// Fake the Win32 Child Process implementation
namespace Oobe
{
    bool Win32ChildProcess::do_start()
    {
        // harmless because the value is never used other than for comparing to nullptr.
        procInfo.hProcess = GetStdHandle(STD_OUTPUT_HANDLE);
        procInfo.dwProcessId = 1;
        procInfo.dwThreadId = 2;
        notifyListener();
        return true;
    }

    void Win32ChildProcess::do_terminate()
    {
        procInfo = PROCESS_INFORMATION{0};
        procInfo.hProcess = nullptr;
    }

    void Win32ChildProcess::do_unsubscribe()
    {
        waiterHandle = nullptr;
    }

    DWORD Win32ChildProcess::do_waitExitSync(DWORD timeoutMs)
    {
        return 0;
    }
}
