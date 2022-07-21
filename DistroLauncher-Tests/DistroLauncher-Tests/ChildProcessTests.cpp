#include "stdafx.h"
#include "gtest/gtest.h"
#include "FakeChildProcessImpl.h"

namespace Testing
{
    using ChildProcess = Oobe::ChildProcessInterface<FakeChildProcess>;
    const auto* fake = L"./does_not_exist";
    TEST(ChildProcess, StartNTerminate)
    {
        ChildProcess process{fake, L""};
        EXPECT_EQ(process.pid(), 0);
        process.start();
        EXPECT_NE(process.pid(), 0);
        process.terminate();
        ASSERT_EQ(process.pid(), 0);
    }
    TEST(ChildProcess, ListenerIsCalled)
    {
        bool called = false;
        ChildProcess process{fake, L""};
        process.setListener([&called] { called = true; });
        EXPECT_FALSE(called);
        process.start();
        ASSERT_TRUE(called);
    }
}
