#include <windows.h>

namespace {
constexpr wchar_t kWindowClass[] = L"ProxyChecker.Foundation";
constexpr wchar_t kTitle[] = L"ProxyChecker - M0 foundation";
constexpr wchar_t kNotice[] =
    L"ProxyChecker native foundation build.\n\n"
    L"Monitoring is NOT implemented in this bootstrap.\n"
    L"See README.md and docs/ROADMAP.md.";

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        SetBkMode(dc, TRANSPARENT);
        DrawTextW(dc, kNotice, -1, &client, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.lpszClassName = kWindowClass;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    if (!RegisterClassExW(&windowClass)) return 1;

    HWND window = CreateWindowExW(0, kWindowClass, kTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 420,
        nullptr, nullptr, instance, nullptr);
    if (!window) return 2;

    ShowWindow(window, showCommand);
    UpdateWindow(window);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
