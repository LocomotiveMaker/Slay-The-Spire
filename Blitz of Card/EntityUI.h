// -----------------------------------------------------------------------------
// @file       EntityUI.h
// @brief      전투 개체(플레이어, 몬스터)의 시각화 및 상호작용 처리 클래스
// -----------------------------------------------------------------------------
#pragma once
#include "UIElement.h"
#include "GameData.h"       // 포함
#include "ProgressBarUI.h" // 포함
#include <string>
#include <vector>

class EntityUI : public UIElement {
private:
    EntityData* data; // 바인딩된 데이터 객체
    bool isPlayer;
    bool isTargeted;
    float hitAnimationTimerSec;
    float attackAnimationTimerSec;
    int anchorCenterX;
    int anchorBottomY;
    int artWidth;
    int artHeight;
    int healthBarWidth;

    ProgressBarUI* healthBar; // 체력바 컴포넌트

    std::vector<std::string> asciiArt;
    CardArchetype playerArchetype;
    const std::vector<std::string>& ResolveCurrentArt() const;
    void RefreshLayout();

public:
    EntityUI(int x, int y, EntityData* entityData, bool isPlayer);
    virtual ~EntityUI();

    void SetTargeted(bool state);
    void TriggerHitAnimation();
    void TriggerAttackAnimation();
    void SetAnchorBottomCenter(int centerX, int bottomY);
    int GetArtCenterX() const { return anchorCenterX; }
    int GetArtTopY() const { return anchorBottomY - artHeight + 1; }
    int GetArtBottomY() const { return anchorBottomY; }
    int GetArtWidth() const { return artWidth; }
    int GetArtHeight() const { return artHeight; }
    int GetHealthBarX() const { return anchorCenterX - (healthBarWidth / 2); }
    int GetHealthBarY() const { return anchorBottomY + 2; }
    int GetHealthBarWidth() const { return healthBarWidth; }

    // UIElement 오버라이드
    bool Update(InputManager& input) override;
    void UpdateAnimation(float deltaTimeSec);
    void Render(ScreenManager& screen) override;
};
