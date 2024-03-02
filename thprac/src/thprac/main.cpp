#define NOMINMAX

#include "thprac_init.h"
#include "thprac_main.h"
#include "thprac_gui_locale.h"
#include "thprac_hook.h"
#include "thprac_cfg.h"
#include "thprac_launcher_main.h"
#include "thprac_log.h"
#include <Windows.h>
#include <psapi.h>
#include <tlhelp32.h>

using namespace THPrac;

bool PrivilegeCheck()
{
    BOOL fRet = FALSE;
    HANDLE hToken = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION Elevation = {};
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) {
            fRet = Elevation.TokenIsElevated;
        }
    }
    if (hToken) {
        CloseHandle(hToken);
    }
    return fRet;
}

static int WINAPI wWinMain(
    [[maybe_unused]] HINSTANCE hInstance,
    [[maybe_unused]] HINSTANCE hPrevInstance,
    PWSTR pCmdLine,
    [[maybe_unused]] int nCmdShow
) {
    CfgSettingsInit();
    HookCtx::VEHInit();

    RemoteInit();
    log_init(true, settings.console_launcher);
    auto thpracMutex = OpenMutexW(SYNCHRONIZE, FALSE, L"thprac - Touhou Practice Tool##mutex");

    if (settings.thprac_admin_rights && !PrivilegeCheck()) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        CloseHandle(thpracMutex);
        ShellExecuteW(nullptr, L"runas", exePath, nullptr, nullptr, SW_SHOW);
        return 0;
    }

    if (settings.check_update_timing == CheckUpdate::Always) {
        MessageBoxW(NULL, L"updates are not implemented", L"Placeholder", MB_ICONINFORMATION);
    }

    if (!settings.dont_search_ongoing_game && FindOngoingGame()) {
        return 0;
    }

    if (settings.existing_game_launch_action != ExistingGameLaunchAction::LauncherAlways
        && FindAndRunGame(settings.existing_game_launch_action == ExistingGameLaunchAction::AlwaysAsk)) {
        return 0;
    }

    if (settings.check_update_timing == CheckUpdate::Always && settings.update_without_confirmation) {
        MessageBoxW(NULL, L"updates are not implemented", L"Placeholder", MB_ICONINFORMATION);
    }

    return GuiLauncherMain(hInstance, nCmdShow);
}
