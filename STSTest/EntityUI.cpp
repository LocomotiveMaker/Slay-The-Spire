// -----------------------------------------------------------------------------
// @file       EntityUI.cpp
// -----------------------------------------------------------------------------
#include "EntityUI.h"

EntityUI::EntityUI(int x, int y, EntityData* entityData, bool isPlayer)
    : UIElement(x, y, 20, 10), data(entityData), isPlayer(isPlayer), isTargeted(false), hitAnimationTimer(0)
{
    // 체력바 객체 동적 할당 (너비 20 고정, y좌표는 아트 하단)
    WORD hpColor = isPlayer ? COLOR_GREEN : COLOR_RED;
    healthBar = new ProgressBarUI(x, y + height - 1, width, &(data->currentHp), &(data->maxHp), "HP", COLOR_WHITE, hpColor);

    if (isPlayer) {
        asciiArt = {
            "       _      ",
            "     _( )_    ",
            "    |     |   ",
            "    |/-\\|   ",
            "      | |     "
        };
    }
    else {
        asciiArt = {
            "    /\\_/\\    ",
            "   ( o.o )   ",
            "    > ^ <    ",
            "   /  _  \\   ",
            "  / /| |\\ \\  "
        };
    }
}

EntityUI::~EntityUI() {
    delete healthBar;
}

void EntityUI::SetTargeted(bool state) {
    isTargeted = state;
}

void EntityUI::TriggerHitAnimation() {
    hitAnimationTimer = 15;
}

bool EntityUI::Update(InputManager& input) {
    if (hitAnimationTimer > 0) {
        hitAnimationTimer--;
    }

    // UI 위치가 변경될 경우 체력바 위치도 동기화 필요 시 여기에 로직 추가
    healthBar->SetPosition(x, y + height - 1);
    healthBar->Update(input);

    return false;
}

void EntityUI::Render(ScreenManager& screen) {
    if (data == nullptr) return;

    int renderX = x;
    if (hitAnimationTimer > 0) {
        renderX += (hitAnimationTimer % 4 < 2) ? 1 : -1;
    }

    WORD color = COLOR_WHITE;
    if (hitAnimationTimer > 0) {
        color = COLOR_RED;
    }
    else if (isTargeted) {
        color = COLOR_YELLOW;
    }

    int artStartY = y + 2;
    for (size_t i = 0; i < asciiArt.size(); ++i) {
        int pad = (width - (int)asciiArt[i].length()) / 2;
        if (pad < 0) pad = 0;
        screen.DrawString(renderX + pad, artStartY + i, asciiArt[i], color);
    }

    // 모듈화된 체력바 렌더링
    healthBar->Render(screen);

    if (isTargeted) {
        screen.DrawString(renderX + 5, y, "[ TARGET ]", COLOR_YELLOW);
    }
}