#include "stdafx.h"
#include "FakeChildProcessImpl.h"

// Fake the Win32 Child Process implementation
namespace Testing
{
    bool FakeChildProcess::do_start()
    {
        // harmless because the value is never used other than for comparing to nullptr.
        procInfo.hProcess = GetStdHandle(STD_OUTPUT_HANDLE);
        procInfo.dwProcessId = 1;
        procInfo.dwThreadId = 2;
        notifyListener();
        return true;
    }

    void FakeChildProcess::do_terminate()
    {
        procInfo = PROCESS_INFORMATION{0};
        procInfo.hProcess = nullptr;
    }

    void FakeChildProcess::do_unsubscribe()
    {
        waiterHandle = nullptr;
    }

    DWORD FakeChildProcess::do_waitExitSync(DWORD timeoutMs)
    {
        return 0;
    }
}
