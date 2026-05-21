// -----------------------------------------------------------------------------
// @file       EntityUI.cpp
// @brief      플레이어/적 엔티티 시각화 구현부
// -----------------------------------------------------------------------------
#include "EntityUI.h"

#include "AsciiArtLibrary.h"
#include "TextLayout.h"

#include <algorithm>

namespace {

constexpr int kPlayerDeckVisualIdBase = 1000;

CardArchetype ResolvePlayerArchetype(const EntityData* data) {
    if (data == nullptr || data->id < kPlayerDeckVisualIdBase) {
        return CardArchetype::None;
    }

    const int archetypeValue = data->id - kPlayerDeckVisualIdBase;
    if (archetypeValue < static_cast<int>(CardArchetype::None) ||
        archetypeValue > static_cast<int>(CardArchetype::Cycle)) {
        return CardArchetype::None;
    }

    return static_cast<CardArchetype>(archetypeValue);
}

const std::vector<std::string>* TryResolveExplicitEnemyArt(int visualArtId) {
    switch (static_cast<AsciiArtId>(visualArtId)) {
    case AsciiArtId::EnemyNormalGoblin:
    case AsciiArtId::EnemyNormalSkeleton:
    case AsciiArtId::EnemyNormalGolem:
    case AsciiArtId::EnemyNormalBat:
    case AsciiArtId::EnemyNormalMushroom:
    case AsciiArtId::EnemyNormalSlime:
    case AsciiArtId::EnemyElite:
    case AsciiArtId::EnemyEliteWitch:
    case AsciiArtId::EnemyEliteReaper:
    case AsciiArtId::EnemyEliteBackturner:
    case AsciiArtId::EnemyBossCentaurus:
    case AsciiArtId::EnemyBossPuppet:
    case AsciiArtId::EnemyBossHydra:
    case AsciiArtId::EnemyBossTitan:
    case AsciiArtId::EnemyBossCrown:
        return &AsciiArtLibrary::Get(static_cast<AsciiArtId>(visualArtId));
    default:
        return nullptr;
    }
}

// Current combat art routing is intentionally simple so shared placeholder art
// can be swapped without touching the rest of the combat UI.
const std::vector<std::string>& ResolveEntityArt(const EntityData* data, bool isPlayer) {
    if (isPlayer) {
        return AsciiArtLibrary::GetPlayerBattle(ResolvePlayerArchetype(data));
    }

    if (data != nullptr) {
        if (const std::vector<std::string>* explicitArt = TryResolveExplicitEnemyArt(data->visualArtId)) {
            return *explicitArt;
        }
    }

    // Temporary convention: 9200+ means boss art.
    if (data != nullptr && data->id >= 9200) {
        return AsciiArtLibrary::GetRandomEnemyBoss();
    }

    // Temporary convention: 9100+ means elite art.
    if (data != nullptr && data->id >= 9100) {
        return AsciiArtLibrary::Get(AsciiArtId::EnemyElite);
    }

    return AsciiArtLibrary::GetRandomEnemyNormal();
}

} // namespace

EntityUI::EntityUI(int x, int y, EntityData* entityData, bool playerEntity)
    : UIElement(0, 0, 24, 10),
    data(entityData),
    isPlayer(playerEntity),
    isTargeted(false),
    hitAnimationTimerSec(0.0f),
    attackAnimationTimerSec(0.0f),
    defendAnimationTimerSec(0.0f),
    anchorCenterX(x),
    anchorBottomY(y),
    artWidth(0),
    artHeight(0),
    healthBarWidth(0),
    healthBar(nullptr),
    playerArchetype(ResolvePlayerArchetype(entityData)) {
    asciiArt = ResolveEntityArt(data, isPlayer);

    const WORD hpColor = isPlayer ? COLOR_GREEN : COLOR_RED;
    healthBar = new ProgressBarUI(0, 0, 20, &(data->currentHp), &(data->maxHp), u8"체력", COLOR_WHITE, hpColor);
    RefreshLayout();
}

EntityUI::~EntityUI() {
    delete healthBar;
}

const std::vector<std::string>& EntityUI::ResolveCurrentArt() const {
    if (!isPlayer) {
        return asciiArt;
    }

    if (data != nullptr && data->currentHp <= 0) {
        return AsciiArtLibrary::GetPlayerDeath(playerArchetype);
    }

    if (hitAnimationTimerSec > 0.0f || defendAnimationTimerSec > 0.0f) {
        return AsciiArtLibrary::GetPlayerDefend(playerArchetype);
    }

    if (attackAnimationTimerSec > 0.0f) {
        return AsciiArtLibrary::GetPlayerAttack(playerArchetype);
    }

    return AsciiArtLibrary::GetPlayerBattle(playerArchetype);
}

void EntityUI::RefreshLayout() {
    const std::vector<std::string>& currentArt = ResolveCurrentArt();
    artWidth = 0;
    artHeight = static_cast<int>(currentArt.size());
    for (const std::string& line : currentArt) {
        artWidth = (std::max)(artWidth, TextLayout::MeasureDisplayWidthUtf8(line));
    }

    healthBarWidth = (std::max)(18, (std::min)(36, artWidth - 4));
    width = (std::max)(artWidth, healthBarWidth);
    height = artHeight + 3;

    const int artTopY = GetArtTopY();
    x = anchorCenterX - (width / 2);
    y = artTopY;
    healthBar->SetBarWidth(healthBarWidth);
    healthBar->SetPosition(anchorCenterX - (healthBarWidth / 2), anchorBottomY + 2);
}

void EntityUI::SetTargeted(bool state) {
    isTargeted = state;
}

void EntityUI::TriggerHitAnimation() {
    hitAnimationTimerSec = 0.5f;
}

void EntityUI::TriggerAttackAnimation(float durationSec) {
    if (!isPlayer) {
        return;
    }

    attackAnimationTimerSec = (std::max)(0.05f, durationSec);
}

void EntityUI::TriggerDefendAnimation(float durationSec) {
    if (!isPlayer) {
        return;
    }

    defendAnimationTimerSec = (std::max)(0.05f, durationSec);
}

void EntityUI::SetAnchorBottomCenter(int centerX, int bottomY) {
    anchorCenterX = centerX;
    anchorBottomY = bottomY;
    RefreshLayout();
}

bool EntityUI::Update(InputManager& input) {
    healthBar->Update(input);
    return false;
}

void EntityUI::UpdateAnimation(float deltaTimeSec) {
    if (hitAnimationTimerSec > 0.0f) {
        hitAnimationTimerSec = (std::max)(0.0f, hitAnimationTimerSec - deltaTimeSec);
    }
    if (attackAnimationTimerSec > 0.0f) {
        attackAnimationTimerSec = (std::max)(0.0f, attackAnimationTimerSec - deltaTimeSec);
    }
    if (defendAnimationTimerSec > 0.0f) {
        defendAnimationTimerSec = (std::max)(0.0f, defendAnimationTimerSec - deltaTimeSec);
    }
}

void EntityUI::Render(ScreenManager& screen) {
    if (data == nullptr) {
        return;
    }

    RefreshLayout();

    int horizontalShake = 0;
    if (hitAnimationTimerSec > 0.0f) {
        const int shakeFrame = static_cast<int>(hitAnimationTimerSec * 60.0f);
        horizontalShake = (shakeFrame % 4 < 2) ? 1 : -1;
    }

    WORD artColor = COLOR_WHITE;
    if (hitAnimationTimerSec > 0.0f) {
        artColor = COLOR_RED;
    }
    else if (isTargeted) {
        artColor = COLOR_YELLOW;
    }

    const int artTopY = GetArtTopY();
    const int artLeftX = anchorCenterX - (artWidth / 2);
    const std::vector<std::string>& currentArt = ResolveCurrentArt();
    for (size_t index = 0; index < currentArt.size(); ++index) {
        const int lineWidth = TextLayout::MeasureDisplayWidthUtf8(currentArt[index]);
        const int drawX = artLeftX + ((artWidth - lineWidth) / 2) + horizontalShake;
        screen.DrawString(drawX, artTopY + static_cast<int>(index), currentArt[index], artColor);
    }

    healthBar->Render(screen);
}
