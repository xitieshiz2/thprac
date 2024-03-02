#include <Windows.h>
#include <d3d9.h>
#include <imgui.h>

#include <memory>

#include "thprac_gui_impl_dx9.h"
#include "thprac_gui_impl_win32.h"
#include "thprac_gui_locale.h"
#include "thprac_cfg.h"

#include "../../resource.h"

typedef HRESULT WINAPI SetProcessDpiAwareness_t(DWORD value);
typedef HRESULT WINAPI GetDpiForMonitor_t(HMONITOR hmonitor, DWORD dpiType, UINT* dpiX, UINT* dpiY);

namespace THPrac {

constexpr unsigned int widthMin = 640;
constexpr unsigned int widthMax = 1280;
constexpr unsigned int heightMin = 480;
constexpr unsigned int heightMax = 960;

std::tuple<IDirect3D9*, IDirect3DDevice9*> D3DCreateDevice(HWND hwnd, D3DPRESENT_PARAMETERS* d3dpp)
{
    HMODULE dll = LoadLibraryW(L"d3d9.dll");
    if (!dll)
        return { nullptr, nullptr };

    auto* pDirect3DCreate9 = (decltype(Direct3DCreate9)*)GetProcAddress(dll, "Direct3DCreate9");

    if (!pDirect3DCreate9)
        return { nullptr, nullptr };

    IDirect3D9* d3d = pDirect3DCreate9(D3D_SDK_VERSION);

    IDirect3DDevice9* d3ddev = nullptr;
    d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, d3dpp, &d3ddev);

    return { d3d, d3ddev };
}

void D3DResetDevice(IDirect3DDevice9* d3ddev, D3DPRESENT_PARAMETERS* d3dpp)
{
    Gui::ImplDX9InvalidateDeviceObjects();
    HRESULT hr = d3ddev->Reset(d3dpp);
    IM_ASSERT(hr != D3DERR_INVALIDCALL);
    Gui::ImplDX9CreateDeviceObjects();
}

bool g_canMove = false;
namespace Gui {
    extern LRESULT ImplWin32WndProcHandlerW(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
}
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (Gui::ImplWin32WndProcHandlerW(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_GETMINMAXINFO: {
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        lpMMI->ptMinTrackSize.x = 300;
        lpMMI->ptMinTrackSize.y = 300;
    }
        return 0;
    case WM_SIZE:
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_NCHITTEST:
        if (g_canMove)
            return HTCAPTION; // allows dragging of the window
        else
            break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

std::tuple<DWORD, DWORD, float> GetScreenSizeAndDPI(HWND hwnd)
{
    DEVMODEW devMod;
    EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &devMod);
    auto dw = devMod.dmPelsWidth, dh = devMod.dmPelsHeight;

    auto shcore = GetModuleHandleW(L"shcore.dll");
    if (!shcore) {
        return { dw, dh, 1.0f };
    }

    auto SetProcDpiAwareness = (SetProcessDpiAwareness_t*)GetProcAddress(shcore, "SetProcessDpiAwareness");
    auto GetDpiForMonitor = (GetDpiForMonitor_t*)GetProcAddress(shcore, "GetDpiForMonitor");

    if (!SetProcDpiAwareness || !GetDpiForMonitor) {
        return { dw, dh, 1.0f };
    }

    UINT dpiX, dpiY;
    SetProcDpiAwareness(1);
    GetDpiForMonitor(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), 0, &dpiX, &dpiY);

    return { dw, dh, dpiX / 96.0f };
}

bool MsgUpdate(HWND hwnd)
{
    MSG msg;
    while (PeekMessageW(&msg, hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (msg.message == WM_QUIT)
            return false;
    }
    return true;
}

bool LauncherNewFrame(HWND hwnd, IDirect3DDevice9* d3ddev, D3DPRESENT_PARAMETERS* d3dpp)
{
    if (!MsgUpdate(hwnd)) {
        return false;
    }
    while (IsIconic(hwnd)) {
        if (d3ddev->Present(nullptr, nullptr, NULL, nullptr) == D3DERR_DEVICELOST
            && d3ddev->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
            D3DResetDevice(d3ddev, d3dpp);
        }
        if (!MsgUpdate(hwnd)) {
            return false;
        }
    }
    ImGuiIO& io = ImGui::GetIO();
    io.KeyCtrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
    io.KeyShift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
    io.KeyAlt = (::GetKeyState(VK_MENU) & 0x8000) != 0;
    io.KeySuper = false;

    Gui::ImplDX9NewFrame();
    Gui::ImplWin32NewFrame(false);
    ImGui::NewFrame();

    return true;
}

void LauncherEndFrame(HWND hwnd, IDirect3DDevice9* d3ddev, D3DPRESENT_PARAMETERS* d3dpp, ImVec2 wndPos, ImVec2 wndSize, bool& moved)
{
    RECT wndRect;
    GetWindowRect(hwnd, &wndRect);
    if ((LONG)wndSize.x != wndRect.right - wndRect.left || (LONG)wndSize.y != wndRect.bottom - wndRect.top) {
        moved = true;
        RECT r = { 0, 0, wndSize.x, wndSize.y };
        AdjustWindowRectEx(&r, WS_POPUP, FALSE, WS_EX_APPWINDOW);
        SetWindowPos(hwnd, NULL,
            wndRect.left + (LONG)wndPos.x, wndRect.top + (LONG)wndPos.y,
            r.right - r.left, r.bottom - r.top,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
    GetClientRect(hwnd, &wndRect);
    RECT rendRect = { (LONG)wndPos.x, (LONG)wndPos.y,
        (LONG)(wndSize.x) + (LONG)(wndPos.x), (LONG)(wndSize.y) + (LONG)(wndPos.y) };
    ImGui::EndFrame();

    d3ddev->SetRenderState(D3DRS_ZENABLE, false);
    d3ddev->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
    d3ddev->SetRenderState(D3DRS_SCISSORTESTENABLE, false);

    d3ddev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFF728C99, 1.0f, 0);

    if (d3ddev->BeginScene() >= 0) {
        ImGui::Render();
        Gui::ImplDX9RenderDrawData(ImGui::GetDrawData());
        d3ddev->EndScene();
    }

    if (d3ddev->Present(&rendRect, nullptr, nullptr, nullptr) == D3DERR_DEVICELOST
        && d3ddev->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
        D3DResetDevice(d3ddev, d3dpp);
    }

    if (moved) {
        if (!(GetKeyState(VK_LBUTTON) < 0)) {
            moved = false;
        }
    }
    ImGui::GetIO().DisplaySize = moved ? ImVec2(5000.0f, 5000.0f) : ImVec2(wndSize.x, wndSize.y);
}

int GuiLauncherMain(void* hInst, int nCmdShow)
{
    HINSTANCE hInstance = (HINSTANCE)hInst;

    WNDCLASSEXW wc = {
        .cbSize = sizeof(wc),
        .lpfnWndProc = WndProc,
        .hInstance = hInstance,
        .hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON1)),
        .lpszClassName = L"thprac launcher",
    };
    ATOM atom = RegisterClassExW(&wc);

    HWND hwnd = CreateWindowW((LPCWSTR)atom, L"thprac launcher", WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, hInstance, nullptr);
    if (!hwnd)
        return 1;

    auto [dispW, dispH, scale] = GetScreenSizeAndDPI(hwnd);

    float wndWidth = 960 * scale;
    float wndHeight = 720 * scale;

    D3DPRESENT_PARAMETERS d3dpp{
        .BackBufferWidth = static_cast<UINT>(widthMax * scale),
        .BackBufferHeight = static_cast<UINT>(heightMax * scale),
        .BackBufferFormat = D3DFMT_UNKNOWN,
        .SwapEffect = D3DSWAPEFFECT_DISCARD,
        .Windowed = TRUE,
        .EnableAutoDepthStencil = TRUE,
        .AutoDepthStencilFormat = D3DFMT_D16,
        .PresentationInterval = D3DPRESENT_INTERVAL_ONE,
    };

    {
        auto wndX = (dispW - wndWidth) / 2;
        auto wndY = (dispH - wndHeight) / 2;
        SetWindowPos(hwnd, NULL, wndX, wndY, wndWidth, wndHeight, 0);
    }

    auto [d3d, d3ddev] = D3DCreateDevice(hwnd, &d3dpp);
    if (!d3ddev) {
        if (d3d)
            d3d->Release();
        DestroyWindow(hwnd);
        return 1;
    }

    d3d = d3d;
    d3ddev = d3ddev;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    ImGui::CreateContext();
    Gui::ImplWin32Init(hwnd);
    Gui::ImplDX9Init(d3ddev);
    Gui::LocaleCreateMergeFont((Gui::locale_t)settings.language, 20.0f * scale);
    ImGui::GetStyle().ScaleAllSizes(scale);

    SetTheme(settings.theme, settings.theme_user.c_str());

    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    io.IniFilename = nullptr;
    style.WindowRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.WindowBorderSize = 0.0f;
    io.ImeWindowHandle = hwnd;
    io.DisplaySize = ImVec2((float)widthMax, (float)heightMax);
    io.KeyMap[ImGuiKey_Tab] = VK_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
    io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
    io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
    io.KeyMap[ImGuiKey_Home] = VK_HOME;
    io.KeyMap[ImGuiKey_End] = VK_END;
    io.KeyMap[ImGuiKey_Insert] = VK_INSERT;
    io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
    io.KeyMap[ImGuiKey_Space] = VK_SPACE;
    io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
    io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
    io.KeyMap[ImGuiKey_KeyPadEnter] = VK_RETURN;
    io.KeyMap[ImGuiKey_A] = 'A';
    io.KeyMap[ImGuiKey_C] = 'C';
    io.KeyMap[ImGuiKey_V] = 'V';
    io.KeyMap[ImGuiKey_X] = 'X';
    io.KeyMap[ImGuiKey_Y] = 'Y';
    io.KeyMap[ImGuiKey_Z] = 'Z';

    // CREATION CODE COMPLETE

    MessageBoxW(NULL, L"Code to check for updates goes here", L"Note", MB_ICONINFORMATION | MB_OK);

    while (LauncherNewFrame(hwnd, d3ddev, &d3dpp)) {
        bool isOpen = true;
        bool isMinimize = false;
        bool canMove = false;
        bool moved = false;

        ImGui::SetNextWindowSizeConstraints(ImVec2(widthMin * scale, heightMin * scale), ImVec2(widthMax * scale, heightMax * scale));
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(960.0f * scale, 720.0f * scale), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.9f);

        ImGui::Begin("thprac launcher", &isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove, &isMinimize);
        if (!isOpen)
            break;
        if (isMinimize) {
            isMinimize = false;
            ShowWindow(hwnd, SW_MINIMIZE);
        }
        canMove = ImGui::IsItemHovered();

        if (ImGui::BeginTabBar("MenuTabBar")) {
            if (ImGui::BeginTabItem(S(THPRAC_GAMES))) {
                ImGui::BeginChild("##games");
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(S(THPRAC_LINKS))) {
                ImGui::BeginChild("##links");
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(S(THPRAC_TOOLS))) {
                ImGui::BeginChild("##tools");
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(S(THPRAC_SETTINGS))) {
                ImGui::BeginChild("##settings");
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImVec2 wndPos = ImGui::GetWindowPos();
        ImVec2 wndSize = ImGui::GetWindowSize();
        canMove &= !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive();
        ImGui::End();

        g_canMove = canMove;
        LauncherEndFrame(hwnd, d3ddev, &d3dpp, wndPos, wndSize, moved);
    }

    Gui::ImplDX9Shutdown();
    Gui::ImplWin32Shutdown();
    ImGui::DestroyContext();
    d3ddev->Release();
    d3d->Release();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

}