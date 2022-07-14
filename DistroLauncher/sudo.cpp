#include "stdafx.h"
#include "sudo.h"

NamedMutex Sudo::sudo_mutex(L"root-user", true);



Sudo::Sudo() noexcept
{
    auto lock = sudo_mutex.lock();
    if (!lock.ok()) {
        status = Status::FAILED_MUTEX;
        return;
    }

    if (const HRESULT hr = Oobe::WslGetDefaultUserAndFlags(default_user_id, wsl_distribution_flags); FAILED(hr)) {
        status = Status::FAILED_GET_USER;
        return;
    }

    constexpr ULONG root_uid = 1;
    if (const HRESULT hr = g_wslApi.WslConfigureDistribution(root_uid, wsl_distribution_flags); FAILED(hr)) {
        status = Status::FAILED_SET_ROOT;
        return;
    }

    mutex_lock = std::move(lock);
    status = Status::OK;
}

Sudo::Sudo(Sudo&& other) noexcept
{
    if (this != &other) {
        mutex_lock = std::exchange(other.mutex_lock, {});
        default_user_id = std::exchange(other.default_user_id, 0);
        wsl_distribution_flags = std::exchange(other.wsl_distribution_flags, {});
        status = std::exchange(other.status, Status::INACTIVE);
    }
}

void Sudo::reset_user() noexcept
{
    if (ok()) {
        g_wslApi.WslConfigureDistribution(default_user_id, wsl_distribution_flags);
        NamedMutex::Lock dummy_lock;
        std::swap(mutex_lock, dummy_lock);
        status = Status::INACTIVE;
    }
}

Sudo::~Sudo()
{
    reset_user();
}

HRESULT Sudo::WslLaunchInteractive(PCWSTR command, BOOL useCurrentWorkingDirectory, DWORD* exitCode)
{
    HRESULT hr = S_FALSE;
    Sudo()
      .and_then([&]() { hr = g_wslApi.WslLaunchInteractive(command, useCurrentWorkingDirectory, exitCode); })
      .or_else([&] { hr = S_FALSE; });

    return hr;
}

HRESULT Sudo::WslLaunch(PCWSTR command, BOOL useCurrentWorkingDirectory, HANDLE stdIn, HANDLE stdOut, HANDLE stdErr,
                        HANDLE* process)
{
    HRESULT hr = S_FALSE;
    Sudo()
      .and_then([&]() noexcept { hr = g_wslApi.WslLaunch(command, useCurrentWorkingDirectory, stdIn, stdOut, stdErr, process); })
      .or_else([&] { hr = S_FALSE; });

    return hr;
}