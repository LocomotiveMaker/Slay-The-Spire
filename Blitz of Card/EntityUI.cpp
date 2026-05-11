// -----------------------------------------------------------------------------
// @file       EntityUI.cpp
// @brief      플레이어/적 엔티티 시각화 구현부
// -----------------------------------------------------------------------------
#include "EntityUI.h"

#include "AsciiArtLibrary.h"
#include "TextLayout.h"

#include <algorithm>

namespace {

// Current combat art routing is intentionally simple so shared placeholder art
// can be swapped without touching the rest of the combat UI.
const std::vector<std::string>& ResolveEntityArt(const EntityData* data, bool isPlayer) {
    if (isPlayer) {
        return AsciiArtLibrary::Get(AsciiArtId::PlayerBattle);
    }

    // Temporary convention: 9200+ means boss art.
    if (data != nullptr && data->id >= 9200) {
        return AsciiArtLibrary::Get(AsciiArtId::EnemyBoss);
    }

    // Temporary convention: 9100+ means elite art.
    if (data != nullptr && data->id >= 9100) {
        return AsciiArtLibrary::Get(AsciiArtId::EnemyElite);
    }

    return AsciiArtLibrary::Get(AsciiArtId::EnemyNormal);
}

} // namespace

EntityUI::EntityUI(int x, int y, EntityData* entityData, bool playerEntity)
    : UIElement(0, 0, 24, 10),
    data(entityData),
    isPlayer(playerEntity),
    isTargeted(false),
    hitAnimationTimer(0),
    anchorCenterX(x),
    anchorBottomY(y),
    artWidth(0),
    artHeight(0),
    healthBarWidth(0),
    healthBar(nullptr) {
    asciiArt = ResolveEntityArt(data, isPlayer);

    const WORD hpColor = isPlayer ? COLOR_GREEN : COLOR_RED;
    healthBar = new ProgressBarUI(0, 0, 20, &(data->currentHp), &(data->maxHp), u8"체력", COLOR_WHITE, hpColor);
    RefreshLayout();
}

EntityUI::~EntityUI() {
    delete healthBar;
}

void EntityUI::RefreshLayout() {
    artWidth = 0;
    artHeight = static_cast<int>(asciiArt.size());
    for (const std::string& line : asciiArt) {
        artWidth = (std::max)(artWidth, TextLayout::MeasureDisplayWidthUtf8(line));
    }

    const int nameWidth = (std::max)(artWidth, TextLayout::MeasureDisplayWidthUtf8(data ? data->name : ""));
    healthBarWidth = (std::max)(18, (std::min)(36, artWidth - 4));
    width = (std::max)(nameWidth, healthBarWidth);
    height = artHeight + 5;

    const int artTopY = GetArtTopY();
    x = anchorCenterX - (width / 2);
    y = artTopY - 2;
    healthBar->SetBarWidth(healthBarWidth);
    healthBar->SetPosition(anchorCenterX - (healthBarWidth / 2), anchorBottomY + 2);
}

void EntityUI::SetTargeted(bool state) {
    isTargeted = state;
}

void EntityUI::TriggerHitAnimation() {
    hitAnimationTimer = 15;
}

void EntityUI::SetAnchorBottomCenter(int centerX, int bottomY) {
    anchorCenterX = centerX;
    anchorBottomY = bottomY;
    RefreshLayout();
}

bool EntityUI::Update(InputManager& input) {
    if (hitAnimationTimer > 0) {
        --hitAnimationTimer;
    }

    healthBar->Update(input);
    return false;
}

void EntityUI::Render(ScreenManager& screen) {
    if (data == nullptr) {
        return;
    }

    RefreshLayout();

    int horizontalShake = 0;
    if (hitAnimationTimer > 0) {
        horizontalShake = (hitAnimationTimer % 4 < 2) ? 1 : -1;
    }

    WORD artColor = COLOR_WHITE;
    if (hitAnimationTimer > 0) {
        artColor = COLOR_RED;
    }
    else if (isTargeted) {
        artColor = COLOR_YELLOW;
    }

    const int artTopY = GetArtTopY();
    const int artLeftX = anchorCenterX - (artWidth / 2);
    for (size_t index = 0; index < asciiArt.size(); ++index) {
        const int lineWidth = TextLayout::MeasureDisplayWidthUtf8(asciiArt[index]);
        const int drawX = artLeftX + ((artWidth - lineWidth) / 2) + horizontalShake;
        screen.DrawString(drawX, artTopY + static_cast<int>(index), asciiArt[index], artColor);
    }

    const std::string headerText = isTargeted ? std::string(u8"[조준 중]") : data->name;
    screen.DrawString(
        anchorCenterX - (width / 2) + horizontalShake,
        artTopY - 2,
        TextLayout::AlignToWidth(TextLayout::Utf8ToWide(headerText), width, TextLayout::HorizontalAlign::Center),
        isTargeted ? COLOR_YELLOW : COLOR_WHITE);

    healthBar->Render(screen);
}
