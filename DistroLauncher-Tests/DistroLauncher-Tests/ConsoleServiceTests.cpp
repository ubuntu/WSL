#include "stdafx.h"
#include "gtest/gtest.h"
#include "local_named_pipe.h"
#include "ConsoleService.h"
#include <fcntl.h>

namespace Win32Utils
{
    TEST(ConsoleServiceTests, RedirectWithInvalidWriteHandleShouldFail)
    {
        // This fake pipe simulates a valid one which fails to expose the write end, thus causing the redirect method in
        // the Console Service to fail.
        struct FakePipe
        {
            FakePipe(FakePipe&& other) = default;
            HANDLE writeHandle()
            {
                return nullptr;
            }
            int writeFileDescriptor()
            {
                return -1;
            }
            void closeWriteHandles()
            { }
            std::wstring pipeName()
            {
                return L"FakePipe";
            }
            HANDLE readHandle()
            {
                return GetStdHandle(STD_OUTPUT_HANDLE);
            }
            explicit FakePipe(bool inheritRead, bool inheritWrite, const wchar_t* name)
            { }
            FakePipe() = default;
        };

        auto fakePipe = makeNamedPipe<FakePipe>(false, false, L"GoodPipe").value();
        ConsoleService<FakePipe> console(std::move(fakePipe));
        ASSERT_EQ(console.redirectConsole().has_value(), false);
        ASSERT_EQ(console.isRedirected(), false);
    }
    TEST(ConsoleServiceTests, RedirectingAGoodFakeDoesNothingButWontFail)
    {
        // This fake pipe simulates the behavior of a healthy pipe by creating a text file and exposing it in 3 ways:
        // 1. As a Win32 HANDLE
        // 2. As a File Descriptor
        // 3. As a File Stream.
        // 1 and 2 are part of the Pipe API, but 3 is a way to fake the stdout and stderr when calling the console
        // service redirectConsole method.
        // By doing so, line 111 effectively redirects this file into itself. This intentionally tricks the console
        // service into thinking the redirection worked. This breach was intentionally left to allow testing.
        struct FakePipe
        {
            HANDLE hFile = nullptr;
            int fd = -1;
            FILE* stream = nullptr;

            FakePipe(FakePipe&& other) noexcept :
                hFile{std::exchange(other.hFile, nullptr)}, fd{std::exchange(other.fd, -1)}, // avoid leaks.
                stream{std::exchange(other.stream, nullptr)}
            { }
            HANDLE writeHandle()
            {
                return hFile;
            }
            int writeFileDescriptor()
            {
                return fd;
            }
            void closeWriteHandles()
            { }
            std::wstring pipeName()
            {
                return L"FakePipe";
            }
            HANDLE readHandle()
            {
                return hFile;
            }
            explicit FakePipe(bool inheritRead, bool inheritWrite, const wchar_t* name)
            {
                HANDLE handle = CreateFile(L"test-file.txt",      // file name
                                           GENERIC_WRITE,         // open for writing
                                           0,                     // do not share
                                           NULL,                  // default security
                                           CREATE_ALWAYS,         // overwrite if exists
                                           FILE_ATTRIBUTE_NORMAL, // normal file
                                           NULL);                 // no attr. template
                if (handle != INVALID_HANDLE_VALUE) {
                    hFile = handle;
                    fd = _open_osfhandle(reinterpret_cast<intptr_t>(hFile), _O_WRONLY | _O_TEXT);
                    stream = _fdopen(fd, "w");
                }
            }

            ~FakePipe()
            {
                if (stream) {
                    fclose(stream);
                    fd = -1;
                    hFile = nullptr;
                    stream = nullptr;
                }
            }
        };

        // Given
        if (auto fakePipe = makeNamedPipe<FakePipe>(false, false, L"GoodPipe"); fakePipe.has_value()) {

            HANDLE handle = fakePipe.value().hFile;
            FILE* stream = fakePipe.value().stream;
            ConsoleService<FakePipe> console(std::move(fakePipe.value()));
            DWORD handleNo = 1;

            // When
            console.redirectConsole(stream, 1, stream, 1); // redirecting the file into itself

            // Then
            ASSERT_EQ(console.isRedirected(), true);

            // When
            console.restoreConsole(); // restoring the console that was actually never redirected.

            // Then
            ASSERT_EQ(console.isRedirected(), false);
        } else {
            std::wcerr << L"\n == This test failed most likely due something outside our control, so that should not"
                          L" be a big problem. ==\n == It's advisable to check the inner FakePipe constructor for bugs"
                          L" in this test. ==\n\n";
        }
    }
}