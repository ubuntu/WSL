#include "stdafx.h"
#include "ApplicationStrategyCommon.h"
#include "WinTuiStrategy.h"

namespace Oobe
{
    HRESULT WinTuiStrategy::do_autoinstall(const std::filesystem::path& autoinstall_file)
    {
        return internal::do_autoinstall(installer, autoinstall_file);
    }

    HRESULT WinTuiStrategy::do_install(Mode ui)
    {
        if (ui == Mode::Gui) {
            wprintf(L"GUI mode is not supported on this platform.\n");
        }
        return internal::install_linux_ui(installer, Mode::Text);
    }

    HRESULT WinTuiStrategy::do_reconfigure()
    {
        return internal::reconfigure_linux_ui(installer);
    }

    void WinTuiStrategy::do_run_splash(bool hideConsole)
    {
        wprintf(L"This device architecture doesn't support running the splash screen.\n");
    }
}
