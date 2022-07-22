#include "stdafx.h"

// Those functions would be complete duplicates if they were methods in the strategy classes.
namespace Oobe::internal
{
    HRESULT do_autoinstall(InstallerController<>& controller, const std::filesystem::path& autoinstall_file)
    {
        auto& stateMachine = controller.sm;

        auto ok = stateMachine.addEvent(InstallerController<>::Events::AutoInstall{autoinstall_file});
        if (!ok) {
            return E_FAIL;
        }
        ok = stateMachine.addEvent(InstallerController<>::Events::BlockOnInstaller{});
        if (!ok) {
            return E_FAIL;
        }
        HRESULT hr = E_NOTIMPL;
        std::visit(internal::overloaded{
                     [&](InstallerController<>::States::Success&) { hr = S_OK; },
                     [&](InstallerController<>::States::UpstreamDefaultInstall& state) { hr = state.hr; },
                     [&](auto&&...) { hr = E_UNEXPECTED; },
                   },
                   ok.value());
        return hr;
    }

    HRESULT install_linux_ui(InstallerController<>& installer, Mode uiMode)
    {
        std::array<InstallerController<>::Event, 3> eventSequence{
          InstallerController<>::Events::InteractiveInstall{uiMode}, InstallerController<>::Events::StartInstaller{},
          InstallerController<>::Events::BlockOnInstaller{}};
        HRESULT hr = E_NOTIMPL;
        for (auto& event : eventSequence) {
            auto ok = installer.sm.addEvent(event);

            // unexpected transition occurred here?
            if (!ok.has_value()) {
                return hr;
            }

            std::visit(internal::overloaded{
                         [&](InstallerController<>::States::Success&) { hr = S_OK; },
                         [&](InstallerController<>::States::UpstreamDefaultInstall& state) { hr = state.hr; },
                         [&](auto&&...) { hr = E_UNEXPECTED; },
                       },
                       ok.value());
        }

        return hr;
    }

    HRESULT reconfigure_linux_ui(Oobe::InstallerController<>& controller)
    {
        std::array<InstallerController<>::Event, 3> eventSequence{InstallerController<>::Events::Reconfig{},
                                                                  InstallerController<>::Events::StartInstaller{},
                                                                  InstallerController<>::Events::BlockOnInstaller{}};

        // for better readability.
        constexpr auto S_CONTINUE = E_NOTIMPL;
        HRESULT hr = S_CONTINUE;

        for (int i = 0; i < eventSequence.size() && hr == S_CONTINUE; ++i) {
            auto ok = controller.sm.addEvent(eventSequence[i]);
            if (!ok) {
                return hr;
            }

            // We can now have:
            // States::UpstreamDefaultInstall on failure;
            // States::Closed -> States::Success (for text mode) or
            // States::Closed -> States::PreparedGui -> States::Ready -> States::Success (for GUI)
            hr = std::visit(internal::overloaded{
                              [&](InstallerController<>::States::Success&) { return S_OK; },
                              [&](InstallerController<>::States::PreparedGui&) { return S_CONTINUE; },
                              [&](InstallerController<>::States::Ready&) { return S_CONTINUE; },
                              [&](InstallerController<>::States::UpstreamDefaultInstall& state) { return state.hr; },
                              [&](auto&&...) { return E_UNEXPECTED; },
                            },
                            ok.value());
        }
        return hr;
    }
}
