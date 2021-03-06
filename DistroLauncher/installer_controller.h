#pragma once
#include <filesystem>
#include <variant>
#include <type_traits>

#include "state_machine.h"

namespace Oobe
{
    /// Following the same principles of the SplashController, this implements the states, events and control of the
    /// Ubuntu Desktop Installer OOBE on WSL. Most of the actual work that requires interacting with the operating
    /// system or with the WSL API has been moved out to a strategy class, which by default is InstallerStrategy class.
    ///
    /// InstallerController admits the following states:
    ///
    /// Closed                  - not running, initial state upon startup.
    /// AutoInstalling          - ready to run the OOBE in `autoinstall` mode. This implies text mode with the
    ///                           assumption
    ///                           that it might be launched in some automated form.
    /// PreparedGui             - preparation steps such as generating the user information used to seed the GUI
    ///                           input fields is complete and detected that OOBE can run in GUI mode.
    /// PreparedTui             - same as above, but TUI mode is required.
    ///                           Both states receive the full command line to invoke the OOBE.
    ///
    /// Ready                   - OOBE was launched asynchronously and it's now ready for interacting with the user.
    ///                           This is useful to let the application do other things while OOBE is preparing its
    ///                           environment and then react when OOBE is ready for the user.
    /// Success                 - OOBE finished successfully. From that point on this controller is meaningless.
    /// UpstreamDefaultInstall  - OOBE cannot run (failed or not existing in this distribution version). From that point
    ///                           on this controller is meaningless
    ///
    ///
    /// The expected state transitions are as follows (using PlantUML notation syntax with the <<choice>> notation
    /// removed for brevity):
    ///
    /// [*] --> Closed
    ///
    /// Closed --> Success                  : Events::Reconfig
    /// note on link: Command line detected request for reconfiguration. OOBE is launched synchronously in text mode.
    ///
    /// /// Closed --> PreparedGui          : Events::Reconfig
    /// note on link: Command line detected request for reconfiguration. OOBE is launched asynchronously in GUI mode.
    ///               In this mode the OOBE will follow the same state transitions as the GUI install.
    ///
    /// Closed --> AutoInstalling           : Events::AutoInstall{autoinstall_file}
    /// note on link: This must be a direct result of command line option `install --autoinstall <autoinstall_file>`
    /// AutoInstalling --> Success          :  Events::BlockOnInstaller
    ///
    /// Closed --> PreparedTui              : Events::InteractiveInstall
    /// note on link: Upon handling that event, text mode requirement was detected.
    ///
    /// Closed --> PreparedGui              : Events::InteractiveInstall
    /// note on link: As above, but GUI mode is allowed.
    ///
    /// PreparedTui / PreparedGui --> Ready : Events::StartInstaller
    /// note on link
    ///     OOBE was started successfully (Subiquity socket is listening). Application must take any required
    ///     actions to ensure user interactivity at this point (restoring the console for TUI, for example).
    ///     This thread becomes blocked until the OOBE is signaled ready or a defined timeout occurs.
    /// end note
    ///
    /// Ready --> Success                    : Events::BlockOnInstaller
    /// note on link: Receiving the BlockOnInstaller event causes this thread will be blocked until the end of the OOBE.
    ///
    /// NOTE: Any of the states above may transition to UpstreamDefaultInstall if a non-recoverable failure occurs.
    /// The application must ensure there is a path in the program to the original upstream terminal experience.
    struct InstallerPolicy;

    template <typename Policy = InstallerPolicy> class InstallerController
    {
      public:
        enum class Mode
        {
            AutoDetect,
            Gui,
            Text
        };
        struct Events
        {
            // This event is meant to be generated by the command line parsing if the OOBE perform an automatic
            // installation seeded by the [autoinstall_file].
            struct AutoInstall
            {
                std::filesystem::path autoinstall_file;
            };

            // The opposite of the AutoInstalling, triggered by launching in install mode where OOBE exists.
            struct InteractiveInstall
            {
                Mode ui = Mode::AutoDetect;
            };

            // Command line parsing equivalent of `launcher config`. Implies the distro is already installed.
            struct Reconfig
            { };

            // Request for launching the installer asynchronously and report when its ready for user interaction.
            struct StartInstaller
            { };

            // Request for the controller to block this thread waiting the installer to finish its job.
            struct BlockOnInstaller
            { };

            using EventVariant =
              std::variant<InteractiveInstall, AutoInstall, Reconfig, StartInstaller, BlockOnInstaller>;
        };
        struct States
        {
            struct Closed;
            struct AutoInstalling;
            struct PreparedGui;
            struct PreparedTui;
            class Ready;
            struct UpstreamDefaultInstall;
            struct Success;
            using StateVariant =
              std::variant<Closed, AutoInstalling, PreparedGui, PreparedTui, Ready, UpstreamDefaultInstall, Success>;

            // [Definitions]

            struct Closed
            {
                // Prepares the distro and the command line for initiating an auto installation.
                StateVariant on_event(typename Events::AutoInstall event)
                {
                    if (!Policy::is_oobe_available()) {
                        return UpstreamDefaultInstall{E_NOTIMPL};
                    }
                    if (!std::filesystem::exists(event.autoinstall_file)) {
                        wprintf(L"Autoinstall file not found. Cannot proceed with auto installation\n");
                        return UpstreamDefaultInstall{ERROR_PATH_NOT_FOUND};
                    }
                    auto source{event.autoinstall_file};
                    std::wstring destination{L"/var/tmp/"};
                    destination.append(source.filename().wstring());
                    if (!Policy::copy_file_into_distro(source, destination)) {
                        wprintf(L"Failed to copy the autoinstall file into the distro file system. Cannot proceed with "
                                L"auto installation\n");
                        return UpstreamDefaultInstall{COMADMIN_E_CANTCOPYFILE};
                    }

                    std::wstring commandLine{Policy::OobeCommand};
                    commandLine.append(L" --text ");
                    commandLine.append(L"--autoinstall ");
                    commandLine.append(destination);

                    return AutoInstalling{commandLine};
                }

                // Decides whether OOBE must be launched in text or GUI mode and seeds it with user information.
                StateVariant on_event(typename Events::InteractiveInstall event)
                {
                    if (!Policy::is_oobe_available()) {
                        return UpstreamDefaultInstall{E_NOTIMPL};
                    }

                    std::wstring commandLine{Policy::OobeCommand};
                    commandLine += Policy::prepare_prefill_info();

                    auto uiMode = event.ui;
                    if (uiMode == Mode::AutoDetect) {
                        if (Policy::must_run_in_text_mode()) {
                            uiMode = Mode::Text;
                        } else {
                            uiMode = Mode::Gui;
                        }
                    }

                    switch (uiMode) {
                    case Mode::Gui:
                        return PreparedGui{commandLine};
                    case Mode::Text:
                        // OOBE runs GUI by default, unless command line option --text is set.
                        commandLine.append(L" --text");
                        return PreparedTui{commandLine};
                    }

                    // this should be unreachable.
                    return UpstreamDefaultInstall{E_UNEXPECTED};
                }

                // Effectively launches the OOBE in reconfiguration variant from start to finish.
                StateVariant on_event(typename Events::Reconfig /*unused*/)
                {
                    if (!Policy::is_oobe_available()) {
                        return UpstreamDefaultInstall{E_NOTIMPL};
                    }

                    std::wstring commandLine{Policy::OobeCommand};
                    if (Policy::must_run_in_text_mode()) {
                        commandLine.append(L" --text");
                        if (auto exitCode = Policy::do_launch_sync(commandLine.c_str()); exitCode != 0) {
                            return UpstreamDefaultInstall{E_FAIL};
                        }

                        return Success{};
                    }

                    return PreparedGui{commandLine};
                }
            };

            struct AutoInstalling
            {
                std::wstring cli;
                StateVariant on_event(typename Events::BlockOnInstaller /*unused*/)
                {
                    if (auto exitCode = Policy::do_launch_sync(cli.c_str()); exitCode != 0) {
                        return UpstreamDefaultInstall{E_FAIL};
                    }
                    Policy::handle_exit_status();
                    return Success{};
                }
            };

            struct PreparedTui
            {
                std::wstring cli;
                Mode mode = Mode::Text;
                StateVariant on_event(typename Events::StartInstaller /*unused*/)
                {
                    const wchar_t* watcher = L"ss -lx | grep subiquity &>/dev/null";
                    HANDLE oobeProcess = Policy::start_installer_async(cli.c_str());
                    if (oobeProcess == nullptr) {
                        return InstallerController::States::UpstreamDefaultInstall{E_HANDLE};
                    }

                    constexpr auto pollAttempts = 50;
                    if (!Policy::poll_success(watcher, pollAttempts, oobeProcess)) {
                        return InstallerController::States::UpstreamDefaultInstall{E_APPLICATION_ACTIVATION_TIMED_OUT};
                    }

                    return InstallerController::States::Ready{oobeProcess, nullptr, INFINITE};
                }
            };

            struct PreparedGui
            {
                std::wstring cli;
                Mode mode = Mode::Gui;
                StateVariant on_event(typename Events::StartInstaller /*unused*/)
                {
                    const wchar_t* watcher = L"ss -lx | grep subiquity &>/dev/null";
                    HANDLE oobeProcess = Policy::start_installer_async(cli.c_str());
                    if (oobeProcess == nullptr) {
                        return InstallerController::States::UpstreamDefaultInstall{E_HANDLE};
                    }

                    HWND rdpWindow = nullptr;
                    constexpr auto numAttempts = 1000;
                    rdpWindow = Policy::try_hiding_installer_window(numAttempts);

                    constexpr auto pollAttempts = 8;
                    if (!Policy::poll_success(watcher, pollAttempts, oobeProcess)) {
                        return InstallerController::States::UpstreamDefaultInstall{E_APPLICATION_ACTIVATION_TIMED_OUT};
                    }

                    return InstallerController::States::Ready{oobeProcess, rdpWindow, INFINITE};
                }
            };

            class Ready
            {
              private:
                HANDLE oobeProcess = nullptr;
                HWND window = nullptr;
                DWORD timeout = 0;

              public:
                Ready(HANDLE oobeProcess, HWND window, DWORD timeout) :
                    oobeProcess{oobeProcess}, window{window}, timeout{timeout} {};

                StateVariant on_event(typename Events::BlockOnInstaller event)
                {
                    if (window != nullptr) {
                        Policy::show_window(window);
                    }
                    // Policy::consume_process must consume the handle otherwise it will leak.
                    if (auto exitCode = Policy::consume_process(oobeProcess, timeout); exitCode != 0) {
                        return UpstreamDefaultInstall{E_ABORT};
                    }
                    Policy::handle_exit_status();
                    return Success{};
                }
            };

            struct UpstreamDefaultInstall
            {
                HRESULT hr = E_NOTIMPL;
            };

            struct Success
            { };
        };

        // The state machine expects the symbol `State` to be visible.
        // The repetition of `typename` is an unfortunate side effect of having this controller as a template. All event
        // and state types are now dependent and cannot be explicitly referred to without the templated type argument.
        using State = typename States::StateVariant;
        using Event = typename Events::EventVariant;

        internal::state_machine<InstallerController> sm;
    };
}