#include "HUDPanel.h"
#include "TextLayout.h"

// 상단 HUD는 값 자체보다도 폭이 흔들리지 않게 유지하는 쪽이 더 중요하다.
HUDPanel::HUDPanel(int width, EntityData* pData, long long* gData, int* fData)
    : UIElement(0, 0, width, 4), playerRef(pData), goldRef(gData), floorRef(fData) {
}

bool HUDPanel::Update(InputManager& input) {
    // Reserved for future HUD interactions.
    (void)input;
    return false;
}

void HUDPanel::Render(ScreenManager& screen) {
    const std::string nameStr = playerRef ? playerRef->name : u8"미확인";
    const std::string hpStr = playerRef
        ? std::string(u8"체력: ") + std::to_string(playerRef->currentHp) + "/" + std::to_string(playerRef->maxHp)
        : std::string(u8"체력: 0/0");
    const std::string goldStr = goldRef
        ? std::string(u8"골드: ") + std::to_string(*goldRef)
        : std::string(u8"골드: 0");
    const std::string floorStr = floorRef
        ? std::string(u8"층: ") + std::to_string(*floorRef)
        : std::string(u8"층: 0");

    const int innerLeft = 2;
    const int innerWidth = width - 4;
    const int laneWidth = innerWidth / 4;
    const int lane1Left = innerLeft;
    const int lane2Left = lane1Left + laneWidth;
    const int lane3Left = lane2Left + laneWidth;
    const int lane4Left = lane3Left + laneWidth;
    const int lane4Width = width - 2 - lane4Left;

    screen.DrawString(
        lane1Left,
        1,
        TextLayout::AlignToWidth(TextLayout::Utf8ToWide(nameStr), laneWidth, TextLayout::HorizontalAlign::Left),
        COLOR_WHITE);
    screen.DrawString(
        lane2Left,
        1,
        TextLayout::AlignToWidth(TextLayout::Utf8ToWide(hpStr), laneWidth, TextLayout::HorizontalAlign::Left),
        FOREGROUND_RED | FOREGROUND_INTENSITY);
    screen.DrawString(
        lane3Left,
        1,
        TextLayout::AlignToWidth(TextLayout::Utf8ToWide(goldStr), laneWidth, TextLayout::HorizontalAlign::Left),
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    screen.DrawString(
        lane4Left,
        1,
        TextLayout::AlignToWidth(TextLayout::Utf8ToWide(floorStr), lane4Width, TextLayout::HorizontalAlign::Right),
        COLOR_WHITE);

    screen.DrawString(0, 3, std::string(width, '='), FOREGROUND_INTENSITY);
}
