/*
 * Copyright (C) 2022 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#pragma once

namespace Oobe
{
    /// Utility object class capable of running any callable in the exit scope.
    /// Be careful when passing lambdas with reference-captures since it will be called during destruction,
    /// thus the referenced objects may have been already destroyed.
    /// Please capture only objects outliving this one!!! Otherwise, make copies.
    /// Also because destructors must be noexcept, so must the callable be as well.
    template <typename OnScopeExit> class ScopeGuard
    {
      public:
        ScopeGuard(OnScopeExit&& callable) : callable{std::forward<OnScopeExit>(callable)} {};
        ~ScopeGuard() noexcept
        {
            static_assert(noexcept(callable()), "Callable must explicitely not throw exceptions.");
            callable();
        }

      private:
        OnScopeExit callable;
    };

    namespace internal
    {
        /// Executes a script to check and disable the correct version of snapd conflicting with the OOBE.
        /// Returns the clean-up command that must run in the end of the calling scope.
        /// Warning: the distro instance may shut down.
        std::wstring TempDisableSnapdImpl(WslApiLoader& api, const std::wstring& distroName);
    }

    /// Checks and temporarily disables the correct version of snapd conflicting with the OOBE.
    /// Returns an object that runs the matching clean up command when the caller scope exits.
    auto TempDisableSnapd(WslApiLoader& api, const std::wstring& distroName)
    {
        auto command = internal::TempDisableSnapdImpl(api, distroName);
        // api is a global and command is moved into the lambda.
        return ScopeGuard([&api, cmd = std::move(command)]() noexcept {
            [[maybe_unused]] DWORD unused;
            api.WslLaunchInteractive(cmd.c_str(), FALSE, &unused);
        });
    }
}
