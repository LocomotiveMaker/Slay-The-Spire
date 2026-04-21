// -----------------------------------------------------------------------------
// @file       HUDPanel.h
// @brief      상단 고정 헤드업 디스플레이 (HP, 골드, 층수 등)
// -----------------------------------------------------------------------------
#pragma once
#include "UIElement.h"
#include "GameData.h"

class HUDPanel : public UIElement {
private:
    EntityData* playerRef;
    long long* goldRef;
    int* floorRef;

public:
    HUDPanel(int width, EntityData* pData, long long* gData, int* fData);

    bool Update(InputManager& input) override;
    void Render(ScreenManager& screen) override;
};