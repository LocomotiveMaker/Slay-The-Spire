// -----------------------------------------------------------------------------
// @file       EntityUI.cpp
// -----------------------------------------------------------------------------
#include "EntityUI.h"
#include "TextLayout.h"

EntityUI::EntityUI(int x, int y, EntityData* entityData, bool isPlayer)
    : UIElement(x, y, 24, 10), data(entityData), isPlayer(isPlayer), isTargeted(false), hitAnimationTimer(0)
{
    if (isPlayer) {
        asciiArt = {
            "       _      ",
            "     _( )_    ",
            "    |     |   ",
            "     |/-\\|    ",
            "      | |     "
        };
    }
    else {
        if (data != nullptr && data->id >= 9200) {
            width = 28;
            asciiArt = {
                "     /\\____/\\\\     ",
                "   _/  o  o  \\\\_   ",
                "  /__   --   __\\\\  ",
                "     | |  | |      ",
                "   __| |__| |__    ",
                "  /___/    \\___\\\\  "
            };
        }
        else if (data != nullptr && data->id >= 9100) {
            width = 26;
            asciiArt = {
                "     /\\__/\\\\      ",
                "    / o  o \\\\     ",
                "   (   --   )     ",
                "    \\\\_==_//      ",
                "   __/  \\__       "
            };
        }
        else {
            asciiArt = {
                "    /\\_/\\\\    ",
                "   ( o.o )   ",
                "    > ^ <    ",
                "   /  _  \\\\   ",
                "  / /| |\\\\ \\\\  "
            };
        }
    }

    height = static_cast<int>(asciiArt.size()) + 5;
    WORD hpColor = isPlayer ? COLOR_GREEN : COLOR_RED;
    healthBar = new ProgressBarUI(x, y + height - 1, width, &(data->currentHp), &(data->maxHp), u8"체력", COLOR_WHITE, hpColor);
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

    const int artStartY = y + 2;
    for (size_t i = 0; i < asciiArt.size(); ++i) {
        int pad = (width - static_cast<int>(asciiArt[i].length())) / 2;
        if (pad < 0) pad = 0;
        screen.DrawString(renderX + pad, artStartY + static_cast<int>(i), asciiArt[i], color);
    }

    healthBar->Render(screen);

    const std::string headerText = isTargeted ? std::string(u8"[ 조준 중 ]") : data->name;
    const std::wstring headerLine = TextLayout::AlignToWidth(
        TextLayout::Utf8ToWide(headerText),
        width,
        TextLayout::HorizontalAlign::Center);
    screen.DrawString(renderX, y, headerLine, isTargeted ? COLOR_YELLOW : COLOR_WHITE);
}
