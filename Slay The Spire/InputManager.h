// -----------------------------------------------------------------------------
// @file       InputManager.h
// @brief      마우스 및 키보드 비동기 입력 처리 (상태 세분화 적용)
// -----------------------------------------------------------------------------
#pragma once
#include <windows.h>

class InputManager {
private:
    HANDLE hConsoleInput;

    int mouseX;
    int mouseY;

    // 현재 프레임 상태
    bool isLeftPressed;
    bool isRightPressed;

    // 이전 프레임 상태 (Down/Up 감지용)
    bool wasLeftPressed;
    bool wasRightPressed;

    int wheelDelta;
    bool isEscPressed;

public:
    InputManager();

    void Update();

    int GetMouseX() const { return mouseX; }
    int GetMouseY() const { return mouseY; }

    // 상태 세분화 Getter
    bool IsLeftClick() const { return isLeftPressed; } // 누르고 있는 상태
    bool IsLeftClickDown() const { return isLeftPressed && !wasLeftPressed; } // 방금 눌린 순간
    bool IsLeftClickUp() const { return !isLeftPressed && wasLeftPressed; }   // 방금 뗀 순간

    int GetWheelDelta() const { return wheelDelta; }
    bool IsEscPressed() const { return isEscPressed; }
};