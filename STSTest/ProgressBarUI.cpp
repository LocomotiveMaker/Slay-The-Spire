// -----------------------------------------------------------------------------
// @file       ProgressBarUI.cpp
// -----------------------------------------------------------------------------
#include "ProgressBarUI.h"

ProgressBarUI::ProgressBarUI(int x, int y, int width, int* curr, int* max, const std::string& prefix, WORD frameCol, WORD fillCol)
    : UIElement(x, y, width, 1), currentValue(curr), maxValue(max), prefixText(prefix), frameColor(frameCol), fillColor(fillCol) {
}

bool ProgressBarUI::Update(InputManager& input) {
    return false; // 진행바 자체는 상호작용(클릭)을 소비하지 않음
}

void ProgressBarUI::Render(ScreenManager& screen) {
    if (currentValue == nullptr || maxValue == nullptr || *maxValue <= 0) return;

    // 비율 계산
    float ratio = static_cast<float>(*currentValue) / static_cast<float>(*maxValue);
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    int fillWidth = static_cast<int>((width - 2) * ratio);

    // 텍스트 준비 (예: "HP 40/80")
    std::string infoText = prefixText + " " + std::to_string(*currentValue) + "/" + std::to_string(*maxValue);
    int textPad = (width - (int)infoText.length()) / 2;
    if (textPad < 0) textPad = 0;

    // 배경 및 테두리 렌더링
    screen.DrawChar(x, y, '[', frameColor);
    screen.DrawChar(x + width - 1, y, ']', frameColor);

    for (int i = 1; i < width - 1; ++i) {
        if (i <= fillWidth) {
            screen.DrawChar(x + i, y, '=', fillColor); // 채워진 영역
        }
        else {
            screen.DrawChar(x + i, y, '-', FOREGROUND_INTENSITY); // 빈 영역 (어두운 회색)
        }
    }

    // 바 중앙에 수치 텍스트 오버레이 (가독성을 위해 흰색 고정)
    screen.DrawString(x + textPad, y, infoText, COLOR_WHITE);
}