// -----------------------------------------------------------------------------
// @file       ProgressBarUI.h
// @brief      체력, 경험치 등 비율 데이터를 시각화하는 범용 진행바 UI
// -----------------------------------------------------------------------------
#pragma once
#include "UIElement.h"
#include <string>

class ProgressBarUI : public UIElement {
private:
    int* currentValue;
    int* maxValue;
    WORD frameColor;
    WORD fillColor;
    std::string prefixText;

public:
    // 데이터의 포인터를 주입받아 실시간으로 상태를 반영함
    ProgressBarUI(int x, int y, int width, int* curr, int* max, const std::string& prefix, WORD frameCol, WORD fillCol);
    void SetBarWidth(int newWidth) { width = newWidth; }

    bool Update(InputManager& input) override;
    void Render(ScreenManager& screen) override;
};
