// -----------------------------------------------------------------------------
// @file       ScreenManager.cpp
// @brief      더블 버퍼링 콘솔 렌더러 구현부
// -----------------------------------------------------------------------------
#include "ScreenManager.h"
#include "TextLayout.h"
#include <algorithm>
#include <cwchar>

namespace {

constexpr short kPreferredWindowedWidth = 160;
constexpr short kPreferredWindowedHeight = 48;
constexpr short kWindowedMarginColumns = 8;
constexpr short kWindowedMarginRows = 4;

RECT GetPreferredMonitorRect(HWND hwnd) {
    HMONITOR monitor = nullptr;

    POINT cursorPos = {};
    if (GetCursorPos(&cursorPos)) {
        monitor = MonitorFromPoint(cursorPos, MONITOR_DEFAULTTONEAREST);
    }

    if (monitor == nullptr && hwnd != nullptr) {
        monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    }

    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);

    if (monitor != nullptr && GetMonitorInfoW(monitor, &monitorInfo)) {
        return monitorInfo.rcMonitor;
    }

    RECT fallbackRect = {};
    fallbackRect.right = GetSystemMetrics(SM_CXSCREEN);
    fallbackRect.bottom = GetSystemMetrics(SM_CYSCREEN);
    return fallbackRect;
}

void ApplyWindowedStyle(HWND hwnd) {
    if (hwnd == nullptr) {
        return;
    }

    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style |= WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
    style &= ~WS_POPUP;
    SetWindowLongPtrW(hwnd, GWL_STYLE, style);

    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
}

void CenterConsoleWindow(HWND hwnd) {
    if (hwnd == nullptr) {
        return;
    }

    RECT windowRect = {};
    if (!GetWindowRect(hwnd, &windowRect)) {
        return;
    }

    const RECT monitorRect = GetPreferredMonitorRect(hwnd);
    const int windowWidth = windowRect.right - windowRect.left;
    const int windowHeight = windowRect.bottom - windowRect.top;
    const int monitorWidth = monitorRect.right - monitorRect.left;
    const int monitorHeight = monitorRect.bottom - monitorRect.top;
    const int targetX = monitorRect.left + ((monitorWidth - windowWidth) / 2);
    const int targetY = monitorRect.top + ((monitorHeight - windowHeight) / 2);

    SetWindowPos(hwnd, HWND_NOTOPMOST, targetX, targetY, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
}

void ApplyBorderlessFullscreen(HWND hwnd) {
    if (hwnd == nullptr) {
        return;
    }

    const RECT monitorRect = GetPreferredMonitorRect(hwnd);

    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    style |= WS_POPUP | WS_VISIBLE;
    SetWindowLongPtrW(hwnd, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE | WS_EX_DLGMODALFRAME);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);

    SetWindowPos(
        hwnd,
        HWND_TOPMOST,
        monitorRect.left,
        monitorRect.top,
        0,
        0,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOSIZE);

    ShowWindow(hwnd, SW_MAXIMIZE);
    UpdateWindow(hwnd);
}

bool ApplyConsoleLayout(HANDLE hConsole, COORD desiredSize, int& width, int& height, COORD& bufferSize, SMALL_RECT& windowRect) {
    SMALL_RECT tempWindow = { 0, 0, 1, 1 };
    SetConsoleWindowInfo(hConsole, TRUE, &tempWindow);

    if (!SetConsoleScreenBufferSize(hConsole, desiredSize)) {
        return false;
    }

    SMALL_RECT exactWindow = { 0, 0, static_cast<short>(desiredSize.X - 1), static_cast<short>(desiredSize.Y - 1) };
    if (!SetConsoleWindowInfo(hConsole, TRUE, &exactWindow)) {
        return false;
    }

    width = desiredSize.X;
    height = desiredSize.Y;
    bufferSize = desiredSize;
    windowRect = exactWindow;
    return true;
}

COORD GetWindowedConsoleSize(HANDLE hConsole) {
    const COORD largestWindow = GetLargestConsoleWindowSize(hConsole);
    if (largestWindow.X <= 0 || largestWindow.Y <= 0) {
        return { kPreferredWindowedWidth, kPreferredWindowedHeight };
    }

    const int maxWidth = (std::max)(80, static_cast<int>(largestWindow.X));
    const int maxHeight = (std::max)(25, static_cast<int>(largestWindow.Y));
    const short targetWidth = static_cast<short>((std::min)(
        maxWidth,
        (std::max)(static_cast<int>(kPreferredWindowedWidth), maxWidth - kWindowedMarginColumns)));
    const short targetHeight = static_cast<short>((std::min)(
        maxHeight,
        (std::max)(static_cast<int>(kPreferredWindowedHeight), maxHeight - kWindowedMarginRows)));

    return { targetWidth, targetHeight };
}

void SyncConsoleViewport(HANDLE hConsole, int& width, int& height, COORD& bufferSize, SMALL_RECT& windowRect) {
    CONSOLE_SCREEN_BUFFER_INFO csbi = {};
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) {
        return;
    }

    const short viewportWidth = static_cast<short>(csbi.srWindow.Right - csbi.srWindow.Left + 1);
    const short viewportHeight = static_cast<short>(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);

    if (viewportWidth <= 0 || viewportHeight <= 0) {
        return;
    }

    SMALL_RECT tempWindow = { 0, 0, 1, 1 };
    SetConsoleWindowInfo(hConsole, TRUE, &tempWindow);

    COORD exactBuffer = { viewportWidth, viewportHeight };
    SetConsoleScreenBufferSize(hConsole, exactBuffer);

    SMALL_RECT exactWindow = { 0, 0, static_cast<short>(viewportWidth - 1), static_cast<short>(viewportHeight - 1) };
    SetConsoleWindowInfo(hConsole, TRUE, &exactWindow);

    width = viewportWidth;
    height = viewportHeight;
    bufferSize = exactBuffer;
    windowRect = exactWindow;
}

} // namespace

ScreenManager::ScreenManager(ScreenMode mode)
    : hConsole(GetStdHandle(STD_OUTPUT_HANDLE)), screenBuffer(nullptr), width(0), height(0) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    ApplyDisplaySettings();
    HWND consoleWindow = GetConsoleWindow();

    if (mode == ScreenMode::Fullscreen) {
        ApplyBorderlessFullscreen(consoleWindow);
        Sleep(100);

        const COORD largestWindow = GetLargestConsoleWindowSize(hConsole);
        const COORD fullscreenSize = {
            static_cast<short>((largestWindow.X > 0) ? largestWindow.X : kPreferredWindowedWidth),
            static_cast<short>((largestWindow.Y > 0) ? largestWindow.Y : kPreferredWindowedHeight)
        };
        ApplyConsoleLayout(hConsole, fullscreenSize, width, height, bufferSize, windowRect);

        ApplyBorderlessFullscreen(consoleWindow);
        Sleep(50);
        SyncConsoleViewport(hConsole, width, height, bufferSize, windowRect);
    }
    else {
        ApplyWindowedStyle(consoleWindow);
        const COORD windowedSize = GetWindowedConsoleSize(hConsole);
        if (!ApplyConsoleLayout(hConsole, windowedSize, width, height, bufferSize, windowRect)) {
            width = kPreferredWindowedWidth;
            height = kPreferredWindowedHeight;
            bufferSize = { static_cast<short>(width), static_cast<short>(height) };
            windowRect = { 0, 0, static_cast<short>(width - 1), static_cast<short>(height - 1) };
        }
        CenterConsoleWindow(consoleWindow);
    }

    ShowScrollBar(consoleWindow, SB_BOTH, FALSE);

    screenBuffer = new CHAR_INFO[width * height];
    Clear();

    CONSOLE_CURSOR_INFO cursorInfo = {};
    cursorInfo.dwSize = 100;
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
}

ScreenManager::~ScreenManager() {
    delete[] screenBuffer;
}

void ScreenManager::ApplyDisplaySettings() {
    SetProcessDPIAware();

    CONSOLE_FONT_INFOEX cfi = {};
    cfi.cbSize = sizeof(cfi);
    cfi.nFont = 0;
    cfi.dwFontSize.X = 0;
    cfi.dwFontSize.Y = 12;
    cfi.FontFamily = FF_DONTCARE;
    cfi.FontWeight = FW_NORMAL;
    wcscpy_s(cfi.FaceName, L"NsimSun");

    SetCurrentConsoleFontEx(hConsole, FALSE, &cfi);
}

void ScreenManager::ResetCell(int x, int y) {
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return;
    }

    const int index = y * width + x;
    screenBuffer[index].Char.UnicodeChar = L' ';
    screenBuffer[index].Attributes = COLOR_WHITE;
}

void ScreenManager::ClearWideGlyphAt(int x, int y) {
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return;
    }

    const int index = y * width + x;
    const WORD attributes = screenBuffer[index].Attributes;

    if ((attributes & COMMON_LVB_LEADING_BYTE) != 0) {
        ResetCell(x + 1, y);
    }
    else if ((attributes & COMMON_LVB_TRAILING_BYTE) != 0) {
        ResetCell(x - 1, y);
    }

    ResetCell(x, y);
}

void ScreenManager::DrawGlyph(int x, int y, wchar_t ch, WORD color, int cellWidth) {
    if (y < 0 || y >= height || cellWidth <= 0) {
        return;
    }

    if (cellWidth == 1) {
        if (x < 0 || x >= width) {
            return;
        }

        ClearWideGlyphAt(x, y);

        const int index = y * width + x;
        screenBuffer[index].Char.UnicodeChar = ch;
        screenBuffer[index].Attributes = color;
        return;
    }

    if (x < 0 || x + 1 >= width) {
        return;
    }

    ClearWideGlyphAt(x, y);
    ClearWideGlyphAt(x + 1, y);

    const int index = y * width + x;
    screenBuffer[index].Char.UnicodeChar = ch;
    screenBuffer[index].Attributes = color | COMMON_LVB_LEADING_BYTE;

    screenBuffer[index + 1].Char.UnicodeChar = L' ';
    screenBuffer[index + 1].Attributes = color | COMMON_LVB_TRAILING_BYTE;
}

bool ScreenManager::IsPureAscii(const std::string& str) const {
    for (unsigned char ch : str) {
        if (ch >= 0x80) {
            return false;
        }
    }
    return true;
}

void ScreenManager::DrawAsciiString(int x, int y, const std::string& str, WORD color) {
    if (y < 0 || y >= height) {
        return;
    }

    int cursorX = x;
    for (unsigned char ch : str) {
        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            break;
        }

        if (cursorX < 0) {
            ++cursorX;
            continue;
        }

        if (cursorX >= width) {
            break;
        }

        const int index = y * width + cursorX;
        screenBuffer[index].Char.UnicodeChar = static_cast<wchar_t>(ch);
        screenBuffer[index].Attributes = color;
        ++cursorX;
    }
}

void ScreenManager::Clear() {
    for (int i = 0; i < width * height; ++i) {
        screenBuffer[i].Char.UnicodeChar = L' ';
        screenBuffer[i].Attributes = COLOR_WHITE;
    }
}

void ScreenManager::DrawChar(int x, int y, char ch, WORD color) {
    DrawChar(x, y, static_cast<wchar_t>(static_cast<unsigned char>(ch)), color);
}

void ScreenManager::DrawChar(int x, int y, wchar_t ch, WORD color) {
    DrawGlyph(x, y, ch, color, TextLayout::GetCellWidth(ch));
}

void ScreenManager::DrawString(int x, int y, const std::string& str, WORD color) {
    if (IsPureAscii(str)) {
        DrawAsciiString(x, y, str, color);
        return;
    }

    DrawString(x, y, TextLayout::Utf8ToWide(str), color);
}

void ScreenManager::DrawString(int x, int y, const std::wstring& str, WORD color) {
    const int startX = x;
    int cursorX = x;
    int cursorY = y;

    for (wchar_t ch : str) {
        if (ch == L'\r') {
            continue;
        }

        if (ch == L'\n') {
            cursorX = startX;
            ++cursorY;
            if (cursorY >= height) {
                break;
            }
            continue;
        }

        const int cellWidth = TextLayout::GetCellWidth(ch);
        if (cellWidth <= 0) {
            continue;
        }

        if (cursorX >= width) {
            break;
        }

        DrawGlyph(cursorX, cursorY, ch, color, cellWidth);
        cursorX += cellWidth;
    }
}

void ScreenManager::Render() {
    COORD characterPos = { 0, 0 };
    WriteConsoleOutputW(hConsole, screenBuffer, bufferSize, characterPos, &windowRect);
}
