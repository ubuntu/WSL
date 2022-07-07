#include "stdafx.h"

Version::Version(const wchar_t* const str)
{
    std::array<unsigned, 4> tmp{0, 0, 0, 0};
    std::size_t read = swscanf_s(str, L"%u.%u.%u.%u", &tmp[0], &tmp[1], &tmp[2], &tmp[3]);
    const auto copy_until = std::next(tmp.cbegin(), read);
    std::copy(tmp.cbegin(), copy_until, v.begin());
}

Version::Version(const std::wstring& str) : Version(str.c_str())
{ }

bool Version::operator==(const Version& other) const
{
    return compare(other) == comparisson::EQUAL;
}

bool Version::operator<(const Version& other) const
{
    return compare(other) == comparisson::LESSER;
}

bool Version::operator>(const Version& other) const
{
    return compare(other) == comparisson::GREATER;
}

bool Version::operator!=(const Version& other) const
{
    return !(*this == other);
}

bool Version::operator<=(const Version& other) const
{
    return !(*this > other);
}

bool Version::operator>=(const Version& other) const
{
    return !(*this < other);
}

void Version::upgrade(const Version& other)
{
    if (*this < other) {
        v = other.v;
    }
}

/// Prints version number without trailing zeros
std::wostream& operator<<(std::wostream& os, const Version& version)
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

Version::comparisson Version::compare(const Version& other) const
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

VersionFile::VersionFile(std::wstring_view linuxpath) :
    linux_path(linuxpath), windows_path(Oobe::WindowsPath(linuxpath))
{ }

bool VersionFile::exists() const
{
    return std::filesystem::exists(windows_path);
}

Version VersionFile::read() const
{
    if (!exists()) {
        return Version{};
    }

    std::wifstream f(windows_path);
    std::wstringstream ss;
    ss << f.rdbuf();
    return Version{ss.str()};
}

HRESULT VersionFile::write(const Version& version)
{
    DWORD exitCode;
    std::wstringstream command;
    command << L"echo " << version << " > " << linux_path;
    return WslLaunchInteractiveAsRoot(command.str().c_str(), 1, &exitCode);
}
