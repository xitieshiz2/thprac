#include <Windows.h>
#include <stdint.h>
#include <format>
#include "thprac_cfg.h"
#include "thprac_utils.h"
#include "utils/utils.h"
#include "utils/wininternal.h"

namespace THPrac {

Settings settings;

std::wstring gDataDir;
rapidjson::Document gCfg;

bool SetTheme(Theme themeId, const wchar_t* userThemeName)
{
    switch (themeId) {
    case Theme::Dark:
        ImGui::StyleColorsDark();
        return false;
    case Theme::Light:
        ImGui::StyleColorsLight();
        return false;
    case Theme::Classic:
        ImGui::StyleColorsClassic();
        return false;
    default:
        // Do nothing
        break;
    }

    if (userThemeName == nullptr) {
        ImGui::StyleColorsDark();
        return true; // TODO: Should we return false instead?
    }

    auto filename = std::format(L"{}\\themes\\{}", gDataDir, userThemeName);
    rapidjson::Document theme;
    auto getColor = [&filename, &theme](const char* const colorName) -> ImVec4 {
        using std::runtime_error, std::format;

        if (!theme.HasMember(colorName))
            throw runtime_error(format("Missing element: {}", colorName));
        if (!theme[colorName].IsArray())
            throw runtime_error(format("Wrong format for {}", colorName));

        const auto& jsonVec = theme[colorName].GetArray();
        if (jsonVec.Size() < 4)
            throw runtime_error(format("Not enough color channels in %s", colorName));

        float channels[4];
        for (int i = 0; i < 4; i++) {
            if (jsonVec[i].IsNumber()) {
                channels[i] = jsonVec[i].GetFloat();
            } else if (jsonVec[i].IsString()) {
                channels[i] = std::stof(jsonVec[i].GetString());
            } else {
                throw runtime_error(format("%s: wrong format", colorName));
            }
        }

        return ImVec4(channels[0], channels[1], channels[2], channels[3]);
    };

    try {
        MappedFile file(filename.c_str());
        if (!file.fileMapView)
            throw std::runtime_error("File not found");
        if (theme.Parse((char*)file.fileMapView, file.fileSize).HasParseError())
            throw std::runtime_error("Invalid JSON (TODO: describe how)");

        ImVec4 colors[ImGuiCol_COUNT];
#define GET_COLOR(color) \
    colors[ImGuiCol_##color] = getColor(#color);

        GET_COLOR(Text);
        GET_COLOR(TextDisabled);
        GET_COLOR(WindowBg);
        GET_COLOR(ChildBg);
        GET_COLOR(PopupBg);
        GET_COLOR(Border);
        GET_COLOR(BorderShadow);
        GET_COLOR(FrameBg);
        GET_COLOR(FrameBgHovered);
        GET_COLOR(FrameBgActive);
        GET_COLOR(TitleBg);
        GET_COLOR(TitleBgActive);
        GET_COLOR(TitleBgCollapsed);
        GET_COLOR(MenuBarBg);
        GET_COLOR(ScrollbarBg);
        GET_COLOR(ScrollbarGrab);
        GET_COLOR(ScrollbarGrabHovered);
        GET_COLOR(ScrollbarGrabActive);
        GET_COLOR(CheckMark);
        GET_COLOR(SliderGrab);
        GET_COLOR(SliderGrabActive);
        GET_COLOR(Button);
        GET_COLOR(ButtonHovered);
        GET_COLOR(ButtonActive);
        GET_COLOR(Header);
        GET_COLOR(HeaderHovered);
        GET_COLOR(HeaderActive);
        GET_COLOR(Separator);
        GET_COLOR(SeparatorHovered);
        GET_COLOR(SeparatorActive);
        GET_COLOR(ResizeGrip);
        GET_COLOR(ResizeGripHovered);
        GET_COLOR(ResizeGripActive);
        GET_COLOR(Tab);
        GET_COLOR(TabHovered);
        GET_COLOR(TabActive);
        GET_COLOR(TabUnfocused);
        GET_COLOR(TabUnfocusedActive);
        GET_COLOR(PlotLines);
        GET_COLOR(PlotLinesHovered);
        GET_COLOR(PlotHistogram);
        GET_COLOR(PlotHistogramHovered);
        GET_COLOR(TableHeaderBg);
        GET_COLOR(TableBorderStrong);
        GET_COLOR(TableBorderLight);
        GET_COLOR(TableRowBg);
        GET_COLOR(TableRowBgAlt);
        GET_COLOR(TextSelectedBg);
        GET_COLOR(DragDropTarget);
        GET_COLOR(NavHighlight);
        GET_COLOR(NavWindowingHighlight);
        GET_COLOR(NavWindowingDimBg);
        GET_COLOR(ModalWindowDimBg);

#undef GET_COLOR
        ImGuiStyle& style = ImGui::GetStyle();
        memcpy(style.Colors, colors, sizeof(colors));
    } catch (std::runtime_error err) {
        std::wstring err_desc = L"Failed to load theme ";
        err_desc.append(filename).append(L"\r\n").append(utf8_to_utf16(err.what()));
        MessageBoxW(nullptr, err_desc.c_str(), L"Failed to load theme", MB_ICONERROR);
        ImGui::StyleColorsDark();
        // TODO: Should we return false at this point?
    }

    return true;
}

void CfgInitDataDir() {
    DWORD localAttrs = GetFileAttributesW(L".thprac_data");

    if (localAttrs != INVALID_FILE_ATTRIBUTES && localAttrs & FILE_ATTRIBUTE_DIRECTORY) {
useCurrentDir:
        // Not dealing with any buffer stuff or GetCurrentDirectory failing.
        // Accessing the actual buffer that GetCurrentDirectory pulls from directly.
        UNICODE_STRING& curdir = CurrentPeb()->ProcessParameters->CurrentDirectory.DosPath;
        size_t l = UNICODE_STRING_LENGTH(curdir);
        gDataDir.reserve(RoundUp(l + constexpr_strlen(L"\\thprac.json"), 16));
        gDataDir.assign(curdir.Buffer, l);
        gDataDir.push_back(L'\\');
        gDataDir.append(L".thprac_data");        
    } else {
        DWORD bufSize = GetEnvironmentVariableW(L"AppData", nullptr, 0);
        if (!bufSize || !--bufSize) {
            MessageBoxW(NULL,
                L"Failed to obtain AppData environment variable"
                L"Defaulting to using .thprac_data in the current folder",
            L"Error", MB_ICONERROR | MB_OK);

            DWORD bakAttrs = GetFileAttributesW(L".thprac_data_backup");
            if (bakAttrs != INVALID_FILE_ATTRIBUTES && bakAttrs & FILE_ATTRIBUTE_DIRECTORY) {
                switch (MessageBoxW(NULL, L".thprac_data_backup detected. Use it?", L"Information", MB_ICONINFORMATION | MB_YESNO)) {
                case IDYES:
                    if (MoveFileW(L".thprac_data_backup", L".thprac_data")) goto useCurrentDir;
                    MessageBoxW(NULL, L"Failed to rename .thprac_data_backup to .thprac_data. Creating new folder", L"Error", MB_ICONERROR | MB_OK);
                default:
                    goto appdataFallbackNewFolder;
                }
            } else {
appdataFallbackNewFolder:
                if (CreateDirectoryW(L".thprac_data", nullptr)) goto useCurrentDir;
                MessageBoxW(NULL, L"Failed to create .thprac_data folder", L"Error", MB_ICONERROR | MB_OK);
                return;
            }
        }

        gDataDir.reserve(RoundUp(bufSize + constexpr_strlen(L"\\thprac\\thprac.json"), 32));
        gDataDir.resize(bufSize);
        GetEnvironmentVariableW(L"AppData", gDataDir.data(), bufSize + 1);
        if (gDataDir[bufSize] != L'\\') {
            gDataDir.push_back(L'\\');
        }
        gDataDir.append(L"thprac");
        CreateDirectoryW(gDataDir.c_str(), nullptr);
    }
}

void CfgSettingsInit() {
    CfgInitDataDir();

    if (!gDataDir.length()) {
        MessageBoxW(NULL, 
            L"Could not find a folder to store thprac data!!!\n\n"
            L"Logs will be written to the folder from which you launched thprac.exe or your game instead\n\n"
            L"No Configuration file will be written, you will be stuck with default settings",
        L"Serious Error", MB_ICONERROR | MB_OK);
        return;
    }
    
    auto jsonPath = std::format(L"{}\\{}", gDataDir, L"thprac.json");
    MappedFile raw(jsonPath.c_str());
    if (!raw.fileMapView) {
        return;
    }

    gCfg.Parse((char*)raw.fileMapView);
    if (gCfg.HasParseError()) {
        return;
    }

    if (!gCfg.HasMember("settings")) {
        return;
    }

    auto& settingsJson = gCfg["settings"];
    if (!settingsJson.IsObject()) {
        return;
    }

    const auto _IntOpt = [](rapidjson::Value& val, unsigned char max, unsigned char* out) {
        if (val.IsInt()) {
            auto ret = val.GetInt();
            if (ret <= max) {
                *out = ret;
            }
        }
    };

#define I(a, def, mx) if (settingsJson.HasMember(#a)) _IntOpt(settingsJson[#a], mx, (unsigned char*)&settings.a)
    I(filter_default, 0, 2);
    I(filename_after_update, 0, 2);
    I(after_launch, 0, 2);
    I(apply_thprac_default, 0, 2);
    I(check_update_timing, 0, 2);
    I(existing_game_launch_action, 0, 2);
    I(theme, 0, 3);
#undef I
    if (settingsJson.HasMember("language")) {
        _IntOpt(settingsJson["language"], 2, (unsigned char*)&settings.language);
    } else {
        settings.language = (Language)Gui::LocaleAutoSet();
    }


#define B(a) if (settingsJson.HasMember(#a)) settings.a = JsonEvalBool(settingsJson[#a])
    B(always_open_launcher);
    B(auto_default_launch);
    B(dont_search_ongoing_game);
    B(check_update);
    B(resizable_window);
    B(reflective_launch);
    B(thprac_admin_rights);
    B(use_relative_path);
    B(update_without_confirmation);
#undef B
}

void CfgWrite() {
}

}