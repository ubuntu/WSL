#include "stdafx.h"
#include "gtest/gtest.h"
#include "local_named_pipe.cpp"

namespace Win32Utils
{
    TEST(LocalNamedPipeTests, PipeNameBusinessRule)
    {
        auto pipe = LocalNamedPipe(false, false, L"test-pipe");
        ASSERT_PRED1([](const std::wstring& name) { return name._Starts_with(L"\\\\.\\pipe\\"); }, pipe.pipeName());
    }
    TEST(LocalNamedPipeTests, FromAnyStringLikeArg)
    {
        std::wstring suffix{L"test-pipe"};
        auto view = std::wstring_view{suffix};
        auto pipe = LocalNamedPipe(false, false, view);
        ASSERT_PRED1([](const HANDLE& read) { return read != INVALID_HANDLE_VALUE; }, pipe.readHandle());
        ASSERT_PRED1([](const std::wstring& name) { return name._Starts_with(L"\\\\.\\pipe\\"); }, pipe.pipeName());
    }
    TEST(LocalNamedPipeTests, NeverReturnClosedHandles)
    {
        auto pipe = LocalNamedPipe(false, false, L"test-pipe");
        auto fd = pipe.writeFileDescriptor();
        auto handle = pipe.writeHandle();
        ASSERT_NE(handle, INVALID_HANDLE_VALUE);
        // After this, both fd and handle are closed.
        pipe.closeWriteHandles();
        DWORD dFlags;
        auto handleResult = GetHandleInformation(handle, &dFlags); // must fail.
        ASSERT_EQ(handleResult, 0);                                // this means failure.
    }
    TEST(LocalNamedPipeTests, AskingForWriteHandlesGiveNewOnes)
    {
        auto pipe = LocalNamedPipe(false, false, L"test-pipe");
        auto fd = pipe.writeFileDescriptor();
        auto handle = pipe.writeHandle();
        ASSERT_NE(handle, INVALID_HANDLE_VALUE);
        pipe.closeWriteHandles();
        ASSERT_NE(handle, pipe.writeHandle());
        ASSERT_NE(fd, pipe.writeFileDescriptor());
    }
}
