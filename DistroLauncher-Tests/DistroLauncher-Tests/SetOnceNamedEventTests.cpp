#include "stdafx.h"
#include "gtest/gtest.h"
#include "SetOnceNamedEvent.h"

namespace Win32Utils
{
    TEST(SetOnceNamedEvent, ValidUntilSet)
    {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        std::wstring name = L"a-global-unique-name-for-today" + std::to_wstring(now.count());
        SetOnceNamedEvent event{name.c_str()};
        EXPECT_TRUE(event.isValid());
        EXPECT_TRUE(event.set());
        ASSERT_FALSE(event.isValid());
    }
}
