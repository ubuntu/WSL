#pragma once
#include "stdafx.h"

namespace Version
{
    [[nodiscard]] PACKAGE_VERSION make(USHORT major, USHORT minor = 0, USHORT build = 0, USHORT revision = 0) noexcept;

    [[nodiscard]] HRESULT current(PACKAGE_VERSION *version);

    [[nodiscard]] constexpr bool left_is_newer(PACKAGE_VERSION left, PACKAGE_VERSION right) noexcept
    {
        return left.Version > right.Version;
    }

    [[nodiscard]] constexpr bool left_is_older(PACKAGE_VERSION left, PACKAGE_VERSION right) noexcept
    {
        return left.Version < right.Version;
    }
}

struct VersionFile
{
    VersionFile(std::wstring_view linuxpath);

    std::filesystem::path linux_path;
    std::filesystem::path windows_path;

    [[nodiscard]] bool exists() const;

    [[nodiscard]] PACKAGE_VERSION read() const;

    HRESULT write(PACKAGE_VERSION version);
};

