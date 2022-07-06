#pragma once
#include "stdafx.h"

struct Version
{
    explicit constexpr Version(unsigned a = 0, unsigned b = 0, unsigned c = 0, unsigned d = 0) noexcept : v{a, b, c, d}
    { }

    explicit Version(const wchar_t* const str)
    {
        std::array<unsigned, 4> tmp{0, 0, 0, 0};
        std::size_t read = swscanf_s(str, L"%u.%u.%u.%u", &tmp[0], &tmp[1], &tmp[2], &tmp[3]);
        const auto copy_until = std::next(tmp.cbegin(), read);
        std::copy(tmp.cbegin(), copy_until, v.begin());
    }

    explicit Version(const std::wstring& str) : Version(str.c_str())
    { }

    bool operator==(const Version& other) const
    {
        return compare(other) == comparisson::EQUAL;
    }

    bool operator<(const Version& other) const
    {
        return compare(other) == comparisson::LESSER;
    }

    bool operator>(const Version& other) const
    {
        return compare(other) == comparisson::GREATER;
    }

    bool operator!=(const Version& other) const
    {
        return !(*this == other);
    }

    bool operator<=(const Version& other) const
    {
        return !(*this > other);
    }

    bool operator>=(const Version& other) const
    {
        return !(*this < other);
    }

    static constexpr Version current() noexcept
    {
        return Version{2210, 0, 88, 0};
    };

    /// Prints version number without trailing zeros
    friend std::wostream& operator<<(std::wostream& os, const Version& version)
    {
        auto second_value = std::next(version.v.cbegin());
        
        auto end_nonzeros = std::find_if(version.v.crbegin(), version.v.crend(), [](auto x) { return x != 0; }).base();
        if (end_nonzeros == version.v.cbegin()) {
            end_nonzeros = version.v.cend(); // No trailing zeros
        }
        
        os << version.v[0];
        std::for_each(second_value, end_nonzeros, [&](auto n) { os << "." << n; });

        return os;
    }

  private:
    std::array<unsigned, 4> v{};

    enum class comparisson
    {
        LESSER,
        EQUAL,
        GREATER
    };

    [[nodiscard]] comparisson compare(const Version& other) const
    {
        const auto diff = std::mismatch(v.cbegin(), v.cend(), other.v.cbegin());
        if (diff.first == v.end()) {
            return comparisson::EQUAL;
        }
        if (*diff.first < *diff.second) {
            return comparisson::LESSER;
        }
        return comparisson::GREATER;
    }
};

struct VersionFile
{
    VersionFile(std::wstring_view linuxpath) : linux_path(linuxpath), windows_path(Oobe::WindowsPath(linuxpath))
    { }

    std::filesystem::path linux_path;
    std::filesystem::path windows_path;

    bool exists() const
    {
        return std::filesystem::exists(windows_path);
    }

    Version read() const
    {
        if (!exists()) {
            return Version{};
        }

        std::wifstream f(windows_path);
        std::wstringstream ss;
        ss << f.rdbuf();
        return Version{ss.str()};
    }

    HRESULT write(const Version& version)
    {
        DWORD exitCode;
        std::wstringstream command;
        command << L"echo " << version << " > " << linux_path;
        return WslLaunchInteractiveAsRoot(command.str().c_str(), 1, &exitCode);
    }
};
