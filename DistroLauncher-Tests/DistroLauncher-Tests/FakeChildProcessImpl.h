#pragma once

#include "ChildProcess.h"

namespace Testing
{
    // Abstracts the OS interaction required to start and manage the child process from the API class. Invariants are
    // held by the parent. Thus everything here is private. Inheritance is used to access the parent class data members.
    class FakeChildProcess : public Oobe::ChildProcessInterface<FakeChildProcess>
    {
        // Effectively starts the process.
        bool do_start();

        // Terminates a running process.
        void do_terminate();

        // Unregisters the wait callback.
        void do_unsubscribe();

        // Blocks the calling thread for [timeoutMs] ms waiting the process to exit. Returns its exit code on success or
        // the waiting error code.
        DWORD do_waitExitSync(DWORD timeoutMs);

        friend class Oobe::ChildProcessInterface<FakeChildProcess>;
    };

}