#include "stdafx.h"
#include "gtest/gtest.h"
// Tests added in a separate file due need to fake win32 APIs.

// This overrides the Win32 CreateFile macro by placing a function with the same signature inside the same namespace as
// the class will be, thus being favorited by ADT.
// That's why we don't specify the global namespace when calling the Win32 functions in the application code.
#undef CreateFile
namespace Test::Win32Utils
{
    HANDLE CreateFile(LPCWSTR lpFileName,
                      DWORD dwDesiredAccess,
                      DWORD dwShareMode,
                      LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                      DWORD dwCreationDisposition,
                      DWORD dwFlagsAndAttributes,
                      HANDLE hTemplateFile)
    {
        return INVALID_HANDLE_VALUE;
    }
}

// This trick makes the linker ignore the object definition in the other test file (LocalNamedPipeTests2.cpp).
// Effectively it is another class because leaves inside another namespace.
namespace Test
{
#include "local_named_pipe.cpp"
}

// The actual tests are here.
namespace Test::Win32Utils
{

    TEST(LocalNamedPipeTests, FailureToCreateFileWontChangeObject)
    {
        auto pipe = LocalNamedPipe(false, false, L"test-pipe");
        ASSERT_EQ(pipe.writeHandle(), nullptr);
        ASSERT_EQ(pipe.writeFileDescriptor(), -1);
    }
}
