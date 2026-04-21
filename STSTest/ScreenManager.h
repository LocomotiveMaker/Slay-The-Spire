// -----------------------------------------------------------------------------
// @file       ScreenManager.h
// @brief      Windows API 기반 더블 버퍼링 콘솔 렌더링 엔진 (복구 버전)
// @note       직접적인 cout 사용을 금지하고, 이 클래스의 버퍼를 통해서만 출력할 것.
// -----------------------------------------------------------------------------
#pragma once
#include <windows.h>
#include <string>

// 자주 사용하는 색상 정의 (비트 연산 조합)
const WORD COLOR_WHITE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
const WORD COLOR_RED = FOREGROUND_RED | FOREGROUND_INTENSITY;
const WORD COLOR_GREEN = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
const WORD COLOR_BLUE = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
const WORD COLOR_YELLOW = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;

class ScreenManager {
private:
    HANDLE hConsole;
    COORD bufferSize;
    SMALL_RECT windowRect;
    CHAR_INFO* screenBuffer; // 2차원 화면을 1차원 배열로 할당할 메모리 공간

    int width;
    int height;

    // 디스플레이 환경 동기화를 위한 내부 헬퍼 함수
    void ApplyDisplaySettings();

public:
    ScreenManager();
    ~ScreenManager();

    // 화면 갱신 및 해상도 정보 
    void Clear();   // 버퍼를 공백으로 초기화
    void Render();  // 메모리 버퍼를 실제 모니터로 한 번에 송출

    // 앵커(Anchor) 헬퍼 함수 - UI 배치 시 절대 좌표 대신 이 함수들을 사용할 것
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    int GetCenterX() const { return width / 2; }
    int GetCenterY() const { return height / 2; }

    // 그리기 함수
    void DrawChar(int x, int y, char ch, WORD color = COLOR_WHITE);
    void DrawString(int x, int y, const std::string& str, WORD color = COLOR_WHITE);
};