#include "InputManager.h"

// 이전 프레임 상태를 함께 기억해서 클릭/해제/홀드 입력을 분리한다.
InputManager::InputManager()
    : mouseX(0),
    mouseY(0),
    isLeftPressed(false),
    isRightPressed(false),
    isEscPressed(false),
    isMapHotkeyPressed(false),
    escPressedDownThisFrame(false),
    mapHotkeyPressedDownThisFrame(false),
    wasLeftPressed(false),
    wasRightPressed(false),
    wheelDelta(0)
{
    hConsoleInput = GetStdHandle(STD_INPUT_HANDLE);

    // 빠른 편집 모드 비활성화 (마우스 드래그 시 콘솔이 멈추는 현상 방지)
    // 동시에 마우스 입력 및 윈도우 크기 조절 이벤트 활성화
    DWORD consoleMode = ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
    SetConsoleMode(hConsoleInput, consoleMode);
}

void InputManager::Update() {
    // 프레임 시작 시 이전 상태 저장
    wasLeftPressed = isLeftPressed;
    wasRightPressed = isRightPressed;
    // 매 프레임마다 휠 델타는 0으로 초기화 (이벤트 발생 시에만 값이 들어감)
    wheelDelta = 0;
    escPressedDownThisFrame = false;
    mapHotkeyPressedDownThisFrame = false;

    DWORD numEvents = 0;
    GetNumberOfConsoleInputEvents(hConsoleInput, &numEvents);

    if (numEvents > 0) {
        INPUT_RECORD* eventBuffer = new INPUT_RECORD[numEvents];
        ReadConsoleInput(hConsoleInput, eventBuffer, numEvents, &numEvents);

        for (DWORD i = 0; i < numEvents; ++i) {
            // 마우스 이벤트 처리
            if (eventBuffer[i].EventType == MOUSE_EVENT) {
                MOUSE_EVENT_RECORD mouseEvent = eventBuffer[i].Event.MouseEvent;

                // 위치 갱신
                mouseX = mouseEvent.dwMousePosition.X;
                mouseY = mouseEvent.dwMousePosition.Y;

                // 클릭 갱신 (비트 연산으로 버튼 상태 파악)
                isLeftPressed = (mouseEvent.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) != 0;
                isRightPressed = (mouseEvent.dwButtonState & RIGHTMOST_BUTTON_PRESSED) != 0;

                // 휠 갱신
                if (mouseEvent.dwEventFlags == MOUSE_WHEELED) {
                    // 상위 워드(HIWORD)에 휠 이동 값이 들어있음 (양수면 Up, 음수면 Down)
                    short wheelVal = HIWORD(mouseEvent.dwButtonState);
                    wheelDelta = (wheelVal > 0) ? 1 : -1;
                }
            }
            // 키보드 이벤트 처리 (ESC 감지용)
            else if (eventBuffer[i].EventType == KEY_EVENT) {
                KEY_EVENT_RECORD keyEvent = eventBuffer[i].Event.KeyEvent;
                if (keyEvent.wVirtualKeyCode == VK_ESCAPE) {
                    if (keyEvent.bKeyDown && !isEscPressed) {
                        escPressedDownThisFrame = true;
                    }
                    isEscPressed = keyEvent.bKeyDown;
                }

                if (keyEvent.wVirtualKeyCode == 'M' || keyEvent.uChar.UnicodeChar == L'ㅡ') {
                    if (keyEvent.bKeyDown && !isMapHotkeyPressed) {
                        mapHotkeyPressedDownThisFrame = true;
                    }
                    isMapHotkeyPressed = keyEvent.bKeyDown;
                }
            }
        }
        delete[] eventBuffer; // 동적 할당 해제
    }
}
