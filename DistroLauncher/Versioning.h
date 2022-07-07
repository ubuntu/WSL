#pragma once
#include "stdafx.h"

struct Version
{
    explicit constexpr Version(unsigned a = 0, unsigned b = 0, unsigned c = 0, unsigned d = 0) noexcept : v{a, b, c, d}
    { }

    explicit Version::Version(const wchar_t* const str);
    explicit Version::Version(const std::wstring& str);

    [[nodiscard]] bool operator==(const Version& other) const;
    [[nodiscard]] bool operator<(const Version& other) const;
    [[nodiscard]] bool operator>(const Version& other) const;
    [[nodiscard]] bool operator!=(const Version& other) const;
    [[nodiscard]] bool operator<=(const Version& other) const;
    [[nodiscard]] bool operator>=(const Version& other) const;

    void upgrade(Version const& other);

    [[nodiscard]] static constexpr Version current() noexcept
    {
        return Version{2210, 0, 88, 0};
    };

    friend std::wostream& operator<<(std::wostream& os, const Version& version);

  private:
    std::array<unsigned, 4> v{};

    enum class comparisson
    {
        LESSER,
        EQUAL,
        GREATER
    };

    [[nodiscard]] comparisson compare(const Version& other) const;
};

struct VersionFile
{
    VersionFile(std::wstring_view linuxpath);

    std::filesystem::path linux_path;
    std::filesystem::path windows_path;

    [[nodiscard]] bool exists() const;

    [[nodiscard]] Version read() const;

    HRESULT write(const Version& version);
};
