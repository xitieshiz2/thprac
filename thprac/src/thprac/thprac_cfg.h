#pragma once

#include <string>
#include <vector>

namespace THPrac {

enum class FilenameAfterUpdate : unsigned char {
	UseDownloaded = 0,
	KeepPrevious = 1,
	UseThprac = 2,
};

enum class AfterLaunchAction : unsigned char {
    Minimize = 0,
	Close = 1,
	Nothing = 2,
};

enum class ApplyThpracDefault : unsigned char {
    Previous = 0,
    Open = 1,
    Close = 2,
};

enum class LinksDisplayDefault : unsigned char {
    None = 0,
    Open = 1,
    Close = 2,
};

enum class Theme : unsigned char {
    Dark = 0,
    Light = 1,
    Classic = 2,
    User = 3,
};

enum class ExistingGameLaunchAction : unsigned char {
    LaunchGame = 0,
    LauncherAlways = 1,
    AlwaysAsk = 2,
};

enum class CheckUpdate : unsigned char {
    OpeningLauncher = 0,
    Always = 1,
    Never = 2,
};

enum class Language : unsigned char {
    Chinese = 0,
    English = 1,
    Japanese = 2,
};

struct Settings {
	// thcrap path actually, that's why it's a wstring
    std::wstring thcrap;
    std::wstring theme_user;
    // Dumb name carried forward forward for backwards compatibility.
    // Type name should be descriptive enough
    LinksDisplayDefault filter_default = LinksDisplayDefault::None;
    FilenameAfterUpdate filename_after_update = FilenameAfterUpdate::UseDownloaded;
    AfterLaunchAction after_launch = AfterLaunchAction::Minimize;
    ApplyThpracDefault apply_thprac_default = ApplyThpracDefault::Previous;
    CheckUpdate check_update_timing = CheckUpdate::OpeningLauncher;
    ExistingGameLaunchAction existing_game_launch_action = ExistingGameLaunchAction::LaunchGame;
    Language language = Language::English;
    Theme theme = Theme::Dark;
    bool always_open_launcher = false;
    bool auto_default_launch = false;
    bool dont_search_ongoing_game = false;
    bool check_update = true;
    bool resizable_window = false;
    bool reflective_launch = false;
    bool thprac_admin_rights = false;
    bool use_relative_path = false;
    bool update_without_confirmation = false;
    bool console_launcher = false;
    bool console_ingame = false;
};

extern Settings settings;

void CfgSettingsInit();
void SetTheme(Theme theme, const wchar_t* theme_user);

}