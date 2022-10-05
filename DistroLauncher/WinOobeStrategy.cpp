#include "stdafx.h"
#include "ApplicationStrategyCommon.h"
#include "WinOobeStrategy.h"

namespace Oobe
{

    std::filesystem::path getOobeExePath()
    {
        return Win32Utils::thisAppRootdir() / appConfig().appName;
    }

    std::filesystem::path getWslConfigPath()
    {
        return Win32Utils::homedir() / L".wslconfig";
    }

    std::wstring createEventName(const wchar_t* const suffix)
    {
        return L"Local\\" + DistributionInfo::Name + L'-' + suffix;
    }

    // Checks the server status file for a successful exit and perform the required post-exit actions.
    HRESULT serverExitStatusCheck()
    {
        if (internal::WslLaunchSuccess(L"grep -E 'DONE|EXITED' /run/subiquity/server-state")) {
            ExitStatusHandling();
            return S_OK;
        }
        return E_FAIL;
    }

    // Concatenates [args...] with the common base CLI to be able to run the OOBE in live run mode.
    // [args...] must contain spaces and quotes and any other metacharacters required to build the command line.
    // Sample result:
    // ubuntu_wsl_setup.exe LR"( --no-dry-run --distro-name="Ubuntu-Preview" ) <args...>"
    //                         |--------------- common cli ------------------|
    template <typename... Streamable> std::wstring makeCli(Streamable&&... args)
    {
        const wchar_t baseCli[] = L" --no-dry-run --distro-name=";
        return concat(baseCli, std::quoted(DistributionInfo::Name), L' ', std::forward<Streamable>(args)...);
    }

    // Detects what UI is supported and runs the appropriate callable supplied.
    // That enables using the same mechanism to run the setup and reconfiguration workflows,
    // since the requirements for running the OOBE won't change between them.
    template <typename OnGui, typename OnTui, typename OnNone>
    HRESULT run_on_autodetect_ui(OnGui onGui, OnTui onTui, OnNone onNone)
    {
        using namespace std::literals::string_view_literals;
        static_assert(std::is_convertible_v<OnGui, std::function<HRESULT()>>,
                      "onGui callback must be a callable returning HRESULT with no arguments");
        static_assert(std::is_convertible_v<OnTui, std::function<HRESULT()>>,
                      "onTui callback must be a callable returning HRESULT with no arguments");
        static_assert(std::is_convertible_v<OnNone, std::function<HRESULT()>>,
                      "onNone callback must be a callable returning HRESULT with no arguments");

        // No further checks if LAUNCHER_FORCE_MODE is set.
        LauncherForceMode forced = environmentForceMode();
        switch (forced) {
        case LauncherForceMode::Invalid:
            [[fallthrough]];
        case LauncherForceMode::Unset:
            break;
        case LauncherForceMode::TextForced:
            return onTui();
        case LauncherForceMode::GuiForced:
            return onGui();
        }

        // WSL 1 cannot mount the Subiquity snap, thus neither GUI nor TUI will be available.
        if (Oobe::internal::WslGetDistroSubsystemVersion() < 2) {
            return onNone();
        }
        // if none of the required snaps exist in the rootfs no UI will be available.
        if (!internal::hasAnyOfSnaps(L"ubuntu-desktop-installer"sv, L"subiquity"sv)) {
            return onNone();
        }

        // if there is no way to boostrap the OOBE snap, no UI available.
        if (!DistributionInfo::isOOBEAvailable()) {
            return onNone();
        }

        // Without localhost forwarding, the OOBE will not be able to connect to the server.
        // TUI mode is still supported.
        if (!Oobe::internal::isLocalhostForwardingEnabled(getWslConfigPath())) {
            return onTui();
        }

        // Otherwise, running the OOBE on Windows is supported. Platform is implied, since this strategy is selected at
        // compile time based on whether the platform supports running the GUI app.
        return onGui();
    }

    bool WslFileBuf::isEmpty() const
    {
        return contents.empty();
    }

    bool WslFileBuf::write() const
    {
        const auto path = WindowsPath(linuxPath);
        std::ofstream file;
        file.open(path.string(), std::ios::ate);
        if (file.fail()) {
            return false;
        }
        file << contents;
        file.close();
        return !file.fail();
    }

    HRESULT Oobe::WinOobeStrategy::do_tui_reconfigure()
    {
        return Oobe::internal::reconfigure_linux_ui(installer);
    }

    HRESULT WinOobeStrategy::do_gui_reconfigure()
    {
        // The OOBE process must not be previously created.
        assert(!oobeProcess.has_value());
        const auto cli = makeCli(L"--reconfigure");
        oobeProcess.emplace(oobeExePath, cli.c_str(), nullptr, nullptr, nullptr);
        if (!oobeProcess.has_value()) {
            return E_UNEXPECTED;
        }

        if (!oobeProcess->start()) {
            return E_APPLICATION_ACTIVATION_EXEC_FAILURE;
        }

        if (oobeProcess->waitExitSync() != 0) {
            return E_FAIL;
        }
        return S_OK;
    }

    HRESULT Oobe::WinOobeStrategy::do_reconfigure()
    {
        // Preserving the same semantics as the previous OOBE, there is no UI mode CLI parameter for reconfiguration,
        // thus always relying on GUI support auto detection. The environment variable can still influence the decision.
        return run_on_autodetect_ui([this] { return do_gui_reconfigure(); }, [this] { return do_tui_reconfigure(); },
                                    [] { return E_NOTIMPL; });
    }

    HRESULT WinOobeStrategy::do_autoinstall(const std::filesystem::path& autoinstall_file)
    {
        return Oobe::internal::do_autoinstall(installer, autoinstall_file);
    }

    void WinOobeStrategy::do_show_console()
    {
        if (!consoleService.has_value()) {
            return;
        }
        std::unique_lock<std::timed_mutex> guard{consoleGuard, std::defer_lock};
        using namespace std::chrono_literals;
        constexpr auto tryLockTimeout = 5s;
        if (!guard.try_lock_for(tryLockTimeout)) {
            wprintf(L"Failed to lock console state for modification. Somebody else is holding the lock.\n");
            return;
        }

        consoleService.value().restoreConsole();
        if (!consoleIsVisible) {
            consoleIsVisible = consoleService.value().showConsoleWindow();
        }
    }

    void WinOobeStrategy::do_close_oobe()
    {
        do_show_console();
        if (splashIsRunning) {
            oobeProcess.value().unsubscribe();
            if (!hCloseOobeEvent.set()) {
                oobeProcess.value().terminate();
                oobeProcess.reset();
            }
            splashIsRunning = false;
        }
    }

    HRESULT WinOobeStrategy::do_install(Mode uiMode)
    {
        // Preserving the same semantics as the previous OOBE, command line takes precendence over the environment
        // variable, only checked if the UI mode CLI parameter is not passed.
        switch (uiMode) {
        case Mode::AutoDetect:
            return run_on_autodetect_ui([this] { return do_gui_install(); }, [this] { return do_tui_install(); },
                                        [] { return E_NOTIMPL; });
        case Mode::Text:
            return do_tui_install();

        case Mode::Gui:
            return do_gui_install();
        }
        return E_INVALIDARG;
    }

    HRESULT WinOobeStrategy::do_gui_install()
    {
        if (!prefill.isEmpty()) {
            prefill.write();
        }

        if (!oobeProcess) {
            return E_NOT_VALID_STATE;
        }

        if (!hRegistrationEvent.set()) {
            do_close_oobe();
            return EVENT_E_USER_EXCEPTION;
        }
        const auto exitCode = oobeProcess.value().waitExitSync();
        do_show_console();
        if (exitCode != 0) {
            return E_FAIL;
        }

        return serverExitStatusCheck();
    }

    HRESULT WinOobeStrategy::do_tui_install()
    {
        std::array<InstallerController<>::Event, 3> eventSequence{
          InstallerController<>::Events::InteractiveInstall{Mode::Text},
          InstallerController<>::Events::StartInstaller{}, InstallerController<>::Events::BlockOnInstaller{}};
        HRESULT hr = E_NOTIMPL;
        for (auto& event : eventSequence) {
            auto ok = installer.sm.addEvent(event);

            // unexpected transition occurred here?
            if (!ok.has_value()) {
                do_close_oobe();
                return hr;
            }

            std::visit(internal::overloaded{
                         [&](InstallerController<>::States::PreparedTui&) { do_show_console(); },
                         [&](InstallerController<>::States::Ready&) { do_close_oobe(); },
                         [&](InstallerController<>::States::Success&) { hr = S_OK; },
                         [&](InstallerController<>::States::UpstreamDefaultInstall& state) {
                             do_show_console();
                             hr = state.hr;
                         },
                         [&](auto&&...) { hr = E_UNEXPECTED; },
                       },
                       ok.value());
        }

        return hr;
    }

    void WinOobeStrategy::do_run_splash(bool hideConsole)
    {
        if (!std::filesystem::exists(oobeExePath)) {
            wprintf(L"OOBE executable [%s] not found.\n", oobeExePath.c_str());
            return;
        }

        auto now = std::chrono::system_clock::now().time_since_epoch();
        auto pipe = Win32Utils::makeNamedPipe(true, false, std::to_wstring(now.count()));
        if (!pipe.has_value()) {
            wprintf(L"Unable to prepare for the execution of the OOBE. Error: %s.\n", pipe.error().c_str());
            return;
        }

        // Even though pipe will be moved soon, the handle is just a pointer value, it won't change or invalidate
        // after moving to another owner.
        consoleReadHandle = pipe.value().readHandle();
        consoleService.emplace(std::move(pipe.value()));
        if (!consoleService.has_value()) {
            wprintf(L"Failed to prepare the console configuration.\n");
            return;
        }

        std::unique_lock<std::timed_mutex> guard{consoleGuard, std::defer_lock};
        using namespace std::chrono_literals;
        constexpr auto tryLockTimeout = 5s;
        if (!guard.try_lock_for(tryLockTimeout)) {
            wprintf(L"Failed to lock console state for modification. Somebody else is holding the lock.\n");
            return;
        }

        consoleService->redirectConsole();

        prefill = WslFileBuf{DistributionInfo::GetPrefillInfoInYaml(), L"/var/log/prefill-system-setup.yaml"};
        const auto cli{prefill.isEmpty() ? makeCli() : makeCli(L" --prefill=", prefill.linuxPath)};

        oobeProcess.emplace(oobeExePath, cli.c_str(), nullptr, consoleReadHandle, nullptr);
        if (!oobeProcess.has_value()) {
            // rollback.
            consoleService->restoreConsole();
            return;
        }

        // The GUI consumes the messages printed to this process stdout.
        // If it dies without this process restoring its console, the next wprintf call would deadlock.
        oobeProcess->setListener([this]() { do_show_console(); });
        if (!oobeProcess->start()) {
            // rollback.
            consoleService->restoreConsole();
            return;
        }

        if (hideConsole) {
            consoleIsVisible = !consoleService->hideConsoleWindow();
        }
        splashIsRunning = true;
    }

    WinOobeStrategy::WinOobeStrategy() :
        oobeExePath{getOobeExePath()}, hRegistrationEvent{createEventName(L"registered").c_str()},
        hCloseOobeEvent{createEventName(L"close-oobe").c_str()}, prefill{}
    {
    }

}
