#include "stdafx.h"
#include "LauncherForceMode.h"

namespace Oobe
{
    constexpr bool inRange(wchar_t value)
    {
        return value >= static_cast<wchar_t>(LauncherForceMode::min) &&
               value <= static_cast<wchar_t>(LauncherForceMode::max);
    }

    LauncherForceMode environmentForceMode()
    {

        // has to consider the NULL-terminating.
        const DWORD expectedSize = 2;
        wchar_t value[expectedSize];
        auto readResult = GetEnvironmentVariable(L"LAUNCHER_FORCE_MODE", value, expectedSize);
        // var unset is not an error.
        const bool unset = (readResult == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND);
        if (unset) {
            return LauncherForceMode::Unset;
        }

        // more than one char + NULL, that's invalid. Like a string or a more than one digit number.
        const bool notASingleChar = (readResult >= expectedSize || value[1] != NULL);
        if (notASingleChar) {
            return LauncherForceMode::Invalid;
        }

        if (!inRange(value[0])) {
            return LauncherForceMode::Invalid;
        }

        return LauncherForceMode{value[0]};
    }
}
