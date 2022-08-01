#pragma once
#include <mutex>
#include "state_machine.h"
#include "installer_policy.h"
#include "installer_controller.h"
#include "local_named_pipe.h"
#include "console_service.h"
#include "ChildProcess.h"
#include "SetOnceNamedEvent.h"

namespace Oobe
{
    using Mode = InstallerController<>::Mode;
    using ChildProcess = ChildProcessInterface<Win32ChildProcess>;

    /// Small buffer for files inside the distro used to delay writing the file to disk from generating its contents.
    struct WslFileBuf
    {
        /// Buffer for the file contents.
        std::string contents;

        /// The full path of the file inside the distro, without the WSL prefix.
        std::filesystem::path linuxPath;

        /// Writes the file to disk.
        bool write() const;

        /// Returns true if this buffer is empty.
        bool isEmpty() const;
    };

    /// Concrete strategy implementation to fulfill the [Application] class that supports running the Flutter OOBE on
    /// Windows. No dependency on WSLg.
    class WinOobeStrategy
    {
      public:
        /// Places the sequence of events to make Subiquity perform an automatic installation.
        /// Does not run the Flutter GUI (not even for the slide show).
        HRESULT do_autoinstall(const std::filesystem::path& autoinstall_file);

        /// Performs an interactive installation.
        /// By default GUI support is checked before launching the OOBE.
        HRESULT do_install(Mode uiMode);

        /// Runs the reconfiguration variant of the Flutter GUI.
        HRESULT do_reconfigure();

        /// Triggers the console redirection and launch the OOBE slide show.
        void do_run_splash(bool hideConsole = false);

        WinOobeStrategy();
        ~WinOobeStrategy() = default;

      private:
        // The Flutter OOBE executable file path.
        std::filesystem::path oobeExePath;
        // It's process manager instance.
        std::optional<ChildProcess> oobeProcess;
        // Named event set upon distro registration detected complete.
        Win32Utils::SetOnceNamedEvent hRegistrationEvent;
        // Named event set to request the OOBE to quit.
        Win32Utils::SetOnceNamedEvent hCloseOobeEvent;
        // This mutex must be acquired before calling ConsoleService::redirectConsole() and
        // ConsoleService::restoreConsole() because those calls may run concurrently with another thread as documented
        // in [ChildProcess].
        std::timed_mutex consoleGuard;
        bool consoleIsVisible = true;
        bool splashIsRunning = false;

        // The controller for running the installer inside Linux. For this strategy that's only used for Subiquity TUI.
        InstallerController<> installer;

        // The handle to the read end of the named pipe that will act as the OOBE stdin.
        HANDLE consoleReadHandle = nullptr;

        // Instance of the console service able to redirect this process stdout to the named pipe that will be read
        // through [consoleReadHandle]
        std::optional<Win32Utils::ConsoleService<Win32Utils::LocalNamedPipe>> consoleService;

        // User information acquired from Windows to populate the GUI during setup. Those contents must be made
        // available to Subiquity as a file and passed to its command line. Since the GUI is responsible for generating
        // the server command line, it needs to know upfront whether there will be contents to pass to the server.
        WslFileBuf prefill;

        /// Restores the console context, which means undo the console redirection and making the window visible.
        void do_show_console();

        /// Notifies the splash controller to quit the application. Console restoration is immediately requested to
        /// prevent leaving the launcher process without a stdout consumer.
        void do_close_oobe();

        /// Runs the Subiquity TUI if the user requested it from command line.
        HRESULT do_tui_install();

        /// Runs the setup variant of Flutter OOBE on Windows.
        HRESULT do_gui_install();

        /// Runs the reconfiguration variant of the Flutter GUI.
        HRESULT do_gui_reconfigure();

        /// Runs the reconfiguration variant of the Subiquity TUI.
        HRESULT do_tui_reconfigure();
    };
}
