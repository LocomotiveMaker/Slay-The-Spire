#include "HUDPanel.h"

HUDPanel::HUDPanel(int width, EntityData* pData, long long* gData, int* fData)
    : UIElement(0, 0, width, 4), playerRef(pData), goldRef(gData), floorRef(fData) {
}

bool HUDPanel::Update(InputManager& input) {
    // HUD 내 특정 아이콘 클릭 처리가 필요할 경우 여기서 구현
    return false;
}

void HUDPanel::Render(ScreenManager& screen) {
    // 1. 상단 정보 텍스트 배치
    std::string nameStr = playerRef ? playerRef->name : "Unknown";
    std::string hpStr = playerRef ? "HP: " + std::to_string(playerRef->currentHp) + "/" + std::to_string(playerRef->maxHp) : "HP: 0/0";
    std::string goldStr = goldRef ? "Gold: " + std::to_string(*goldRef) : "Gold: 0";
    std::string floorStr = floorRef ? "Floor: " + std::to_string(*floorRef) : "Floor: 0";

    screen.DrawString(2, 1, nameStr, COLOR_WHITE);
    screen.DrawString(20, 1, hpStr, FOREGROUND_RED | FOREGROUND_INTENSITY);
    screen.DrawString(45, 1, goldStr, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY); // Yellow
    screen.DrawString(width - 20, 1, floorStr, COLOR_WHITE);

    // 2. 구분선 (Separator) 렌더링
    std::string separator(width, '=');
    screen.DrawString(0, 3, separator, FOREGROUND_INTENSITY); // 회색 선
}