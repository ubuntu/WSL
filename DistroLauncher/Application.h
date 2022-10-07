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

#include "extended_cli_parser.h"

namespace Oobe
{
    static constexpr int EXIT_OOBE_NO_FALLBACK = 123;
    /// Provides the high level API through which the controllers and services are integrated to run (or not) the OOBE
    /// according to the hardware platform capabilities and the result of the command line parsing. Platform specific
    /// code is provided by the Strategy template type.
    template <typename Strategy = DefaultAppStrategy> class Application
    {
      private:
        Strategy impl_;
        internal::Opts arg;
        bool isAutoInstall()
        {
            return std::holds_alternative<internal::AutoInstall>(arg);
        }
        bool isReconfig()
        {
            return std::holds_alternative<internal::Reconfig>(arg);
        }

        bool canHideConsole() const
        {
            return std::holds_alternative<internal::ManifestMatchedInstall>(arg);
        }

      public:
        /// Returns true if the command line parsing doesn't require running the OOBE.
        /// That implies partial command line parsing, with the required actions deferred to the upstream code.
        bool shouldSkipInstaller()
        {
            return std::holds_alternative<std::monostate>(arg);
        }

        /// Constructs the initial state of the application. The [arguments] vector is assumed not to hold the argv[0]
        /// parameter - i.e. the program name. It is passed as a mutating reference to ensure the extended command line
        /// options will be trimmed out to avoid confusion in the upstream "command line parsing code".
        Application(std::vector<std::wstring_view>& arguments) : arg{internal::parseExtendedOptions(arguments)}
        { }
        Application(const Application& other) = delete;
        Application(Application&& other) = delete;
        ~Application() = default;

        /// Invokes the OOBE to perform a setup, which can be an automatic or interactive installation, decided by the
        /// output of the command line parsing. In other scenarios this returns E_INVALIDARG to signify some CLI parsing
        /// problem or wrong call-site.
        HRESULT setup()
        {
            using namespace internal;
            wprintf(L"Unpacking is complete!\n");
            HRESULT hr =
              std::visit(internal::overloaded{
                           [&](AutoInstall& option) { return impl_.do_autoinstall(option.autoInstallFile); },
                           [&](ManifestMatchedInstall& option) { return impl_.do_install(Mode::AutoDetect); },
                           [&](InstallDefault& option) { return impl_.do_install(Mode::AutoDetect); },
                           [&](InstallOnlyDefault& option) { return impl_.do_install(Mode::AutoDetect); },
                           [&](InteractiveInstallOnly<OobeGui>& option) { return impl_.do_install(Mode::Gui); },
                           [&](InteractiveInstallOnly<OobeTui>& option) { return impl_.do_install(Mode::Text); },
                           [&](InteractiveInstallShell<OobeGui>& option) { return impl_.do_install(Mode::Gui); },
                           [&](InteractiveInstallShell<OobeTui>& option) { return impl_.do_install(Mode::Text); },
                           [&](auto&& option) { return E_INVALIDARG; }, // for the cases not treated by this function.
                         },
                         arg);

            if (FAILED(hr) && hr != E_NOTIMPL && hr != E_INVALIDARG) {
                wprintf(L"Installer did not complete successfully.\n");
                Helpers::PrintErrorMessage(hr);
                if (appConfig().mustSkipFallback) {
                    exit(EXIT_OOBE_NO_FALLBACK);
                }
                wprintf(L"Applying fallback method.\n");
            }

            return hr;
        }

        /// Invokes the OOBE in reconfiguration mode.
        /// In order to trigger reconfiguration with the command `launcher.exe config`, the call-site has to be
        /// different from where the setup() function is called. Thus, checking whether the command line parsing
        /// actually resulted in Reconfig mode in the first place helps preventing undesired exceptions. In other
        /// scenarios this returns E_INVALIDARG to signify some CLI parsing problem or wrong call-site.
        HRESULT reconfigure()
        {
            if (isReconfig()) {
                return impl_.do_reconfigure();
            }

            return E_INVALIDARG;
        }

        /// Runs the splash application if the OOBE is enabled in interactive mode by the command line parsing.
        void runSplash()
        {
            if (isAutoInstall() || shouldSkipInstaller()) {
                return;
            }
            impl_.do_run_splash(canHideConsole());
        }
    };
}
