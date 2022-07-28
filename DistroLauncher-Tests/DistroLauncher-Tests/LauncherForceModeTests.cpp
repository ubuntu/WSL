#include "stdafx.h"
#include "gtest/gtest.h"
#include "LauncherForceMode.h"

namespace Oobe
{
    constexpr wchar_t varName[] = L"LAUNCHER_FORCE_MODE";
    TEST(LauncherForceMode, Undefined)
    {
        SetEnvironmentVariable(varName, NULL);
        ASSERT_EQ(environmentForceMode(), LauncherForceMode::Unset);
    }
    TEST(LauncherForceMode, Invalids)
    {
        SetEnvironmentVariable(varName, L"7");
        ASSERT_EQ(environmentForceMode(), LauncherForceMode::Invalid);
        SetEnvironmentVariable(varName, L"002");
        ASSERT_EQ(environmentForceMode(), LauncherForceMode::Invalid);
        SetEnvironmentVariable(varName, L"a");
        ASSERT_EQ(environmentForceMode(), LauncherForceMode::Invalid);
        SetEnvironmentVariable(varName, L"A");
        ASSERT_EQ(environmentForceMode(), LauncherForceMode::Invalid);
    }
    TEST(LauncherForceMode, Valids)
    {
        SetEnvironmentVariable(varName, L"1");
        ASSERT_EQ(environmentForceMode(), LauncherForceMode::TextForced);
        SetEnvironmentVariable(varName, L"2");
        ASSERT_EQ(environmentForceMode(), LauncherForceMode::GuiForced);
        SetEnvironmentVariable(varName, L"0");
        ASSERT_EQ(environmentForceMode(), LauncherForceMode::Unset);
    }
}
