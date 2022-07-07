#include "stdafx.h"

namespace Version
{
    PACKAGE_VERSION make(USHORT major, USHORT minor, USHORT build, USHORT revision) noexcept
    {
        PACKAGE_VERSION v{};
        v.Revision = revision;
        v.Build = build;
        v.Minor = minor;
        v.Major = major;
        return v;
    }

    HRESULT current(PACKAGE_VERSION *version)
    {
        UINT32 buffer_len = sizeof(PACKAGE_ID);
        std::vector<BYTE> pkg_id_buffer(buffer_len, 0);

        LONG hr = GetCurrentPackageId(&buffer_len, pkg_id_buffer.data());

        if (hr == ERROR_INSUFFICIENT_BUFFER) {
            pkg_id_buffer.resize(buffer_len);
            LONG hr = GetCurrentPackageId(&buffer_len, pkg_id_buffer.data());
        }

        if (hr != ERROR_SUCCESS) {
            return hr;
        }

        *version = reinterpret_cast<PACKAGE_ID *>(pkg_id_buffer.data())->version;
        return hr;
    }

}

VersionFile::VersionFile(std::wstring_view linuxpath) :
    linux_path(linuxpath), windows_path(Oobe::WindowsPath(linuxpath))
{ }

bool VersionFile::exists() const
{
    return std::filesystem::exists(windows_path);
}

PACKAGE_VERSION VersionFile::read() const
{
    if (!exists()) {
        return Version::make(0);
    }

    std::wifstream f(windows_path);
    
    PACKAGE_VERSION v{};
    f >> std::hex >> v.Version;
    return v;
}

HRESULT VersionFile::write(PACKAGE_VERSION version)
{
    DWORD exitCode;
    std::wstringstream command;
    command << L"echo " << std::hex << version.Version << " > " << linux_path;
    return WslLaunchInteractiveAsRoot(command.str().c_str(), 1, &exitCode);
}
