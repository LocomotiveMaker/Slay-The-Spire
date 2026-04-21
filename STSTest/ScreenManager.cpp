// -----------------------------------------------------------------------------
// @file       ScreenManager.cpp
// -----------------------------------------------------------------------------
#include "ScreenManager.h"
#include <cwchar> // wcscpy_s 사용을 위함

ScreenManager::ScreenManager() {
    // 1. 환경 동기화 설정 적용 (DPI 및 폰트 고정)
    ApplyDisplaySettings();

    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    // 2. 창 최대화 모드 적용
    HWND hwnd = GetConsoleWindow();
    ShowWindow(hwnd, SW_MAXIMIZE);
    Sleep(100); // 윈도우 창이 최대화될 때까지 잠시 대기

    // 3. 현재 화면 해상도(글자 칸 수) 획득
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

    // 4. 버퍼 사이즈 및 렌더링 영역 설정
    bufferSize = { (short)width, (short)height };
    windowRect = { 0, 0, (short)(width - 1), (short)(height - 1) };
    SetConsoleScreenBufferSize(hConsole, bufferSize);

    // 5. 메모리 버퍼 할당
    screenBuffer = new CHAR_INFO[width * height];
    Clear();

    // 6. 커서 숨기기 (모든 창 설정이 끝난 후 가장 마지막에 실행해야 안 풀림)
    CONSOLE_CURSOR_INFO cursorInfo;
    cursorInfo.dwSize = 100;
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
}

ScreenManager::~ScreenManager() {
    delete[] screenBuffer; // 메모리 누수 방지
}

void ScreenManager::ApplyDisplaySettings() {
    // Windows 디스플레이 배율(125%, 150% 등) 무시 및 실제 픽셀 매칭 강제
    SetProcessDPIAware();

    HANDLE tempHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_FONT_INFOEX cfi;
    cfi.cbSize = sizeof(cfi);
    cfi.nFont = 0;
    cfi.dwFontSize.X = 0;  // 너비 0 (높이에 맞춰 시스템이 자동 비율 조정)
    cfi.dwFontSize.Y = 12; // 1920x1080 기준 가로 약 200칸 이상을 확보하는 표준 수치

    cfi.FontFamily = FF_DONTCARE;
    cfi.FontWeight = FW_NORMAL;

    // 고정폭 폰트 명시적 지정
    wcscpy_s(cfi.FaceName, L"NsimSun");

    SetCurrentConsoleFontEx(tempHandle, FALSE, &cfi);
}

void ScreenManager::Clear() {
    // 전체 버퍼를 순회하며 공백(' ')과 기본 색상으로 초기화
    for (int i = 0; i < width * height; ++i) {
        screenBuffer[i].Char.AsciiChar = ' ';
        screenBuffer[i].Attributes = COLOR_WHITE;
    }
}

void ScreenManager::DrawChar(int x, int y, char ch, WORD color) {
    // 안전장치(Clipping): 화면 밖을 벗어나는 좌표는 그리지 않음
    if (x >= 0 && x < width && y >= 0 && y < height) {
        int index = y * width + x;
        screenBuffer[index].Char.AsciiChar = ch;
        screenBuffer[index].Attributes = color;
    }
}

void ScreenManager::DrawString(int x, int y, const std::string& str, WORD color) {
    for (size_t i = 0; i < str.length(); ++i) {
        DrawChar(x + i, y, str[i], color);
    }
}

void ScreenManager::Render() {
    // 백버퍼(screenBuffer)의 내용을 실제 콘솔 창으로 덮어씌움
    COORD characterPos = { 0, 0 };
    WriteConsoleOutputA(hConsole, screenBuffer, bufferSize, characterPos, &windowRect);
}