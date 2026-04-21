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
    int hitAnimationTimer;

    ProgressBarUI* healthBar; // 체력바 컴포넌트

    std::vector<std::string> asciiArt;

public:
    EntityUI(int x, int y, EntityData* entityData, bool isPlayer);
    virtual ~EntityUI();

    void SetTargeted(bool state);
    void TriggerHitAnimation();

    // UIElement 오버라이드
    bool Update(InputManager& input) override;
    void Render(ScreenManager& screen) override;
};