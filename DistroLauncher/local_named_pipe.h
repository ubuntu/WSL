#pragma once
namespace Win32Utils
{
    /**
     * Encapsulates the Win32 named pipe which can be inherited by child processes into an RAII object to simplify
     * its usage. This pipe is said local in the sense that both client and server stays in the same application.
     * This is useful for situations like logging or console redirection. Server end is inbound access, i.e. it's ment
     * to be read-only while client is a write-only file. The client handle could be inherited by a child process as
     * it's stdout and the process creating the pipe could read it to display in a text component, for example.
     * The opposite is also viable, the process creating the pipe could redirect it's stdout and stderr to the write
     * handle and let a child process inherit the read handle as stdin, implementing a unix shell pipe like
     * process chain.
     *
     * CLASS INVARIANTS
     *
     * At any point in time an instance of this class must:
     *
     * Have both hWrite and writeFd valid.
     * Invalidate one of the memebers above must invalidate the other simultaneously.
     * hRead must stay valid for the whole object lifecycle.
     * szPipeName must start with \\.\pipe\
     * write end getters must always return valid handles.
     *
     */
    class LocalNamedPipe
    {
      public:
        // Constructs a named pipe from a pipe name or any combination of arguments that can build a wstring.
        // Pipe name will be automatically prefixed with pipeNamePrefix per Win32 rules.
        // Set inheritRead to true if the read handle should be inheritable. The same applies for inheritWrite.
        template <typename... Args>
        explicit LocalNamedPipe(bool inheritRead, bool inheritWrite, Args&&... args) :
            readSA{sizeof(SECURITY_ATTRIBUTES), nullptr, true}, writeSA{sizeof(SECURITY_ATTRIBUTES), nullptr, true},
            inheritWrite{inheritWrite}
        {
            szPipeName.append(std::forward<Args>(args)...);
            hRead = CreateNamedPipe(szPipeName.c_str(),
                                    PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                                    PIPE_TYPE_MESSAGE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                    PIPE_UNLIMITED_INSTANCES,
                                    0,
                                    0,
                                    0,
                                    &readSA);

            SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, static_cast<BOOL>(inheritRead));
        }
        const HANDLE readHandle() const
        {
            return hRead;
        }

        const std::wstring& pipeName() const
        {
            return szPipeName;
        }

        // To make piping stdout and stderr work, one will need to close the write end of this pipe after assing the
        // standard file descriptors.
        void closeWriteHandles();

        // Getter to the OS Handle of the write end of the pipe.
        const HANDLE writeHandle();
        // Getter to the OS Handle of the write end of the pipe.
        const int writeFileDescriptor();

        ~LocalNamedPipe();

      private:
        // Those are delicate to be exposed. Leaving the write end of this pipe fly all over the places could result
        // in application crashes. Thats why the methods writeHandle() and closeWriteHandles() exist.
        void openWriteEnd();
        HANDLE hWrite = INVALID_HANDLE_VALUE;
        int writeFd = -1;
        bool inheritWrite;

        // Nobody will need to access those outside of this class.
        SECURITY_ATTRIBUTES readSA, writeSA;
        // Those could be public, but to keep the API consistent, they are private.
        HANDLE hRead = INVALID_HANDLE_VALUE;
        // This naming convention (\\\\.\\pipe\\) is a business rule of Win32 named pipes.
        std::wstring szPipeName{L"\\\\.\\pipe\\"};
    };

}
