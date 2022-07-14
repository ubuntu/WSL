#include "stdafx.h"
#include "gtest/gtest.h"
#include "local_named_pipe.h"

namespace Win32Utils
{
    TEST(PipeNameValidation, RemovesBackslash)
    {
        auto name = pipeNameFrom(L"tes\\t-\\pipe");
        ASSERT_EQ(std::wstring(L"\\\\.\\pipe\\test-pipe"), name);
    }
    TEST(PipeNameValidation, ReturnsDefaultIfEmpty)
    {
        auto name = pipeNameFrom(L"");
        ASSERT_EQ(std::wstring(L"\\\\.\\pipe\\LOCAL"), name);
        name = pipeNameFrom(L"\\");
        ASSERT_EQ(std::wstring(L"\\\\.\\pipe\\LOCAL"), name);
    }
    TEST(PipeNameValidation, TrimsToMaxLength)
    {
        auto name = pipeNameFrom(static_cast<size_t>(300), L't');
        ASSERT_EQ(name.length(), 256);
    }
    TEST(LocalNamedPipeTests, PipeNameBusinessRule)
    {
        auto pipe = LocalNamedPipe(false, false, L"test-pipe");
        ASSERT_PRED1([](const std::wstring& name) { return name._Starts_with(L"\\\\.\\pipe\\"); }, pipe.pipeName());
    }
    TEST(LocalNamedPipeTests, FromAnyStringLikeArg1)
    {
        std::wstring suffix{L"test-pipe"};
        auto view = std::wstring_view{suffix};
        auto pipe = LocalNamedPipe(false, false, view);
        ASSERT_PRED1([](const HANDLE& read) { return read != nullptr; }, pipe.readHandle());
        ASSERT_PRED1([](const std::wstring& name) { return name._Starts_with(L"\\\\.\\pipe\\"); }, pipe.pipeName());
    }
    TEST(LocalNamedPipeTests, FromAnyStringLikeArg2)
    {
        // Args [static_cast<size_t>(5), L'u'] in string constructor means the char 'u' repeated 5 times.
        auto pipe = LocalNamedPipe(false, false, static_cast<size_t>(5), L'u');
        ASSERT_PRED1([](const HANDLE& read) { return read != nullptr; }, pipe.readHandle());
        ASSERT_EQ(L"\\\\.\\pipe\\uuuuu", pipe.pipeName());
    }
    TEST(LocalNamedPipeTests, NeverReturnClosedHandles)
    {
        auto pipe = LocalNamedPipe(false, false, L"test-pipe");
        auto fd = pipe.writeFileDescriptor();
        auto handle = pipe.writeHandle();
        ASSERT_NE(handle, nullptr);
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
        ASSERT_NE(handle, nullptr);
        pipe.closeWriteHandles();
        ASSERT_NE(handle, pipe.writeHandle());
        ASSERT_NE(fd, pipe.writeFileDescriptor());
    }

    TEST(MakeNamedPipeTests, TestFailedPipeCreation)
    {
        using namespace std::string_literals;
        // The trick below shows the benefit of using templates for passing dependences.
        struct FakePipe
        {
            std::wstring pipeName()
            {
                return L"FakePipe";
            }
            HANDLE readHandle()
            {
                return nullptr;
            }
            explicit FakePipe(bool inheritRead, bool inheritWrite, const wchar_t* name)
            { }
        };

        auto failure = makeNamedPipe<FakePipe>(false, false, L"GoodPipe");
        ASSERT_EQ(failure.has_value(), false);
        ASSERT_PRED1([](const std::wstring& value) { return value.find(L"FakePipe") != std::wstring::npos; },
                     failure.error());
    }
    TEST(MakeNamedPipeTests, PipeBehaviorShouldNotChange)
    { // The environment should never fail, but if it does this test should still pass if the error handling is
      // consistent with the original design.
        auto pipe = makeNamedPipe(false, false, L"test-pipe");
        if (pipe.has_value()) {
            ASSERT_PRED1([](const std::wstring& name) { return name._Starts_with(L"\\\\.\\pipe\\"); },
                         pipe.value().pipeName());
        } else {
            // Just in case that ever happens we'll have something different to notice.
            std::wcerr << pipe.error() << L'\n';
            ASSERT_PRED1(
              [](const std::wstring& value) { return value.find(L"\\\\.\\pipe\\test-pipe") != std::wstring::npos; },
              pipe.error());
        }
    }
}
