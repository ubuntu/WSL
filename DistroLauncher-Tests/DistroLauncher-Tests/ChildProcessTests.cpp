#include "stdafx.h"
#include "gtest/gtest.h"
#include "ChildProcess.h"

namespace Oobe
{
    const auto* fake = L"./do_not_exists";
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
