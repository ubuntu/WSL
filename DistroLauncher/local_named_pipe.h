#pragma once
namespace Win32Utils
{
    /**
     * Encapsulates the Win32 named pipe which can be inherited by child processes into an RAII object to simplify
     * its usage. This pipe is said local in the sense that both client and server stays in the same application.
     * This is useful for situations like logging or console redirection. Server end is inbound access, i.e. it's meant
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
     * A factory function (makeNamedPipe()) is offered as a good way to create a named pipe as an expected object. Shall
     * the creation fail, the function will return a message with some context about the failure.
     */

    // Returns an always valid pipe name. The arguments this function accepts must match one std::wstring
    // constructor and if so will be appended to the prefix demanded by Win32 API (\\.\pipe\). There are more business
    // rules involving the named pipe names. Win32 Docs says:
    // _"The pipename part of the name can include any character
    // other than a backslash, including numbers and special characters. The entire pipe name string can be up to 256
    // characters long. Pipe names are not case sensitive."_
    // Consider room for the 9 characters for the prefix (\\.\pipe\) MAX_SUFFIX_SIZE must be: 247.
    // See: <https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createnamedpipea>
    template <typename... Args> std::wstring pipeNameFrom(Args&&... args)
    {
        constexpr auto MAX_SUFFIX_LENGTH = 247;
        std::wstring name(std::forward<Args>(args)...);
        name.erase(std::remove(name.begin(), name.end(), '\\'), name.end());
        if (name.length() > MAX_SUFFIX_LENGTH) {
            name.erase(name.begin() + MAX_SUFFIX_LENGTH, name.end()); // erase is [first, last)
        }
        assert(name.length() <= MAX_SUFFIX_LENGTH);
        if (name.empty()) {
            name.insert(0, L"LOCAL");
        }
        name.insert(0, L"\\\\.\\pipe\\");
        return name;
    }

    class LocalNamedPipe
    {
      private:
        void openWriteEnd();
        // This naming convention (\\\\.\\pipe\\) is a business rule of Win32 named pipes.
        std::wstring szPipeName;
        // Nobody will need to access those outside of this class.
        SECURITY_ATTRIBUTES readSA, writeSA;

        HANDLE hRead = nullptr;

        // Those are delicate to be exposed. Leaving the write end of this pipe fly all over the places could result
        // in application crashes. Thats why the methods writeHandle() and closeWriteHandles() exist.
        HANDLE hWrite = nullptr;
        int writeFd = -1;
        bool inheritWrite;

      public:
        // Constructs a named pipe from a pipe name string or any combination of arguments that can build a wstring.
        // Pipe name is validated according to the Win32 API rules.
        // The boolean flags [inheritRead] and [inheritWrite] dictate whether the respective HANDLE can be inherited by
        // a child process, since the main usage of named pipes envolves inter process communication.
        template <typename... Args>
        explicit LocalNamedPipe(bool inheritRead, bool inheritWrite, Args&&... args) :
            readSA{sizeof(SECURITY_ATTRIBUTES), nullptr, true}, writeSA{sizeof(SECURITY_ATTRIBUTES), nullptr, true},
            inheritWrite{inheritWrite}, szPipeName{pipeNameFrom(std::forward<Args>(args)...)}
        {
            auto handle = CreateNamedPipe(szPipeName.c_str(),
                                          PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                                          PIPE_TYPE_MESSAGE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                          PIPE_UNLIMITED_INSTANCES,
                                          0,
                                          0,
                                          0,
                                          &readSA);
            // That's the exact value returned by the function call above if it fails.
            // NOLINTNEXTLINE(performance-no-int-to-ptr)
            if (handle != INVALID_HANDLE_VALUE) {
                hRead = handle; // only assign if valid. Otherwise, hRead will stay a nullptr.
                SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, static_cast<BOOL>(inheritRead));
            }
        }
        // Move constructor is necessary for the expected idiom to work correctly with classes that handles resources
        // (pointers, file descriptors, OS handles...
        // The moved-from object must be left in a "zeroed-out" state, so when its destructor gets called there is no
        // risk of closing a handle being used by other objects.
        LocalNamedPipe(LocalNamedPipe&& other) noexcept :
            szPipeName{std::exchange(other.szPipeName, std::wstring{})},
            readSA{std::exchange(other.readSA, {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE})},
            writeSA{std::exchange(other.writeSA, {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE})}, // zeroing everything.
            hRead{std::exchange(other.hRead, nullptr)}, hWrite{std::exchange(other.hWrite, nullptr)},
            writeFd{std::exchange(other.writeFd, -1)}, inheritWrite{std::exchange(other.inheritWrite, false)}
        { }

        HANDLE readHandle() const
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
        HANDLE writeHandle();
        // Getter to the OS Handle of the write end of the pipe.
        int writeFileDescriptor();

        ~LocalNamedPipe();
    };

    // Factory which returns valid and healthy named pipes or a unexpected<string> message.
    // The unusual signature of this function allows extensibility.
    // The default usage may not even notice there are templates involved:
    // ```cpp
    // auto pipe = makeNamedPipe(false, false, L"test-pipe");
    // if (pipe.has_value()) {
    //    do_something_with_the_pipe(...);
    // }
    //```
    //
    // At the same time, it allows extensibility, like:
    // ```cpp
    // auto pipe = makeNamedPipe<FakePipe>(false, false, L"GoodPipe");
    // ASSERT_SOMETHING_ABOUT_THE_FAKE_PIPE(...);
    //```
    template <class Pipe = LocalNamedPipe, typename... Args>
    nonstd::expected<Pipe, std::wstring> makeNamedPipe(bool inheritRead, bool inheritWrite, Args&&... args)
    {
        auto pipe = Pipe(inheritRead, inheritWrite, std::forward<Args>(args)...);
        if (pipe.readHandle() != nullptr) {
            return pipe;
        }
        // if failed:
        auto msg = pipe.pipeName();
        msg.insert(0, L"Failed to create a pipe. Pipe name was: ");
        msg.append(L" . Error code: ");
        msg.append(std::to_wstring(GetLastError()));
        return nonstd::make_unexpected(std::move(msg));
    }

}
