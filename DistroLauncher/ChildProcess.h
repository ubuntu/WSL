#pragma once
/*
 *  Abstract
 *  --------
 *  A proxy for a child process that enables reacting to its termination from the outside.
 *  Useful when dealing with processes that may be closed at any moment by the user, like GUI ones.
 *
 *
 *  Usage
 *  --------
 *  Define the process executable, CLI arguments and handles to the standard console devices:
 *
 *  ```
 *  ChildProcess child{L"notepad.exe",L"",nullptr,nullptr,nullptr}; //nullptr handles disable console interaction.
 *  ```
 *
 *  Set a listener if and when you want to be notified about an extraneous process termination:
 *
 *  ```
 *  child.setListener([]{std::cout << "Who killed my child?\n";});
 *  ```
 *
 *  > The function object above will be owned by this class. The listener will be called on another thread, be careful
 * with synchronization. Throwing exceptions there is also not a very good idea.
 *  > Setting a listener for a process that never runs effectively does nothing.
 *
 *  Kill it if you're upset with its behavior for some reason:
 *
 * ```
 *  child.terminate();
 * ```
 *
 * Or just let the destructor do it. Move the child to an outer scope if you don't want the process to be killed.
 *
 * Or else wait on the process to exit:
 *
 * ```
 * DWORD exitCode = child.waitExitSync(); // without any arguments it will wait forever.
 * ```
 *
 */

namespace Oobe
{

    template <typename ProcessAPI> class ChildProcessInterface
    {
      public:
        // Alias to just enforce the communication that the supplied callback will run on another thread.
        using CallInOtherThread = std::function<void()>;

        // Starts the process.
        bool start()
        {
            return static_cast<ProcessAPI*>(this)->do_start();
        }

        /// Sets the listener that will be called back once (and if) the process ends from the outside.
        /// IMPORTANT NOTE ON MULTITHREADING: [onClose] will be invoked by another thread, thus any precaution
        /// required to ensure safe concurrent call must be taken by the [onClose] callable implementation itself and
        /// any functions it calls inside its body. That's the reason for the horrible type name.
        template <typename CallableInOtherThread> void setListener(CallableInOtherThread&& onCloseCallback)
        {
            onCloseListener.emplace(std::forward<CallableInOtherThread>(onCloseCallback));
        }

        // Terminates the running process forcebly.
        void terminate()
        {
            unsubscribe();
            static_cast<ProcessAPI*>(this)->do_terminate();
        }
        DWORD waitExitSync(DWORD timeoutMs = INFINITE)
        {
            return static_cast<ProcessAPI*>(this)->do_waitExitSync(timeoutMs);
        }

        // Unregisters the wait callback, if set.
        void unsubscribe()
        {
            if (waiterHandle != nullptr) {
                static_cast<ProcessAPI*>(this)->do_unsubscribe();
                waiterHandle = nullptr;
            }
        }

        DWORD pid() const
        {
            return procInfo.dwProcessId;
        }

        DWORD threadId() const
        {
            return procInfo.dwProcessId;
        }

        constexpr ChildProcessInterface(const std::filesystem::path& exePath, const std::wstring& args,
                                        HANDLE stdErr = nullptr, HANDLE stdIn = nullptr, HANDLE stdOut = nullptr) :
            executable{exePath},
            arguments{args}
        {
            startInfo.cb = sizeof(STARTUPINFO);
            startInfo.hStdError = stdErr;
            startInfo.hStdInput = stdIn;
            startInfo.hStdOutput = stdOut;
            startInfo.dwFlags |= STARTF_USESTDHANDLES;
        }

      protected:
        HANDLE waiterHandle;
        // The ID of the process's main thread.
        PROCESS_INFORMATION procInfo = {0};
        STARTUPINFO startInfo = {0};
        SECURITY_ATTRIBUTES sa = {0};

        // The program's path.
        std::filesystem::path executable;

        // It's command line arguments.
        std::wstring arguments;

        // A callback that will be executed in another thread in case the user closes the window.
        std::optional<CallInOtherThread> onCloseListener;

        // Executes the listener callback, if set. Not meant to be called from the wild.
        void notifyListener()
        {
            if (onCloseListener.has_value()) {
                onCloseListener.value()();
            }
        }
    };

    // Abstracts the OS interaction required to start and manage the child process from the API class. Invariants are
    // held by the parent. Thus everything here is private. Inheritance is used to access the parent class data members.
    class Win32ChildProcess : public ChildProcessInterface<Win32ChildProcess>
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

        // Destroys the required handles.
        void destroy();

        // Call-once function which must be called only by the OS in reaction to process termination from the outside.
        // Since the onCloseListener member is initialized before the process runs and (must) never be touched since
        // then, it's safe to assume no need for synchronization whatsoever in accessing it. Any need for
        // synchronization must be taken care of by the callable itself.
        static void onClose(void* data, BOOLEAN /*unused*/);

        friend class ChildProcessInterface<Win32ChildProcess>;
    };

    using ChildProcess = ChildProcessInterface<Win32ChildProcess>;
}
