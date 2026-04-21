// -----------------------------------------------------------------------------
// @file       ProgressBarUI.cpp
// -----------------------------------------------------------------------------
#include "ProgressBarUI.h"
#include "TextLayout.h"
#include <cmath>

ProgressBarUI::ProgressBarUI(int x, int y, int width, int* curr, int* max, const std::string& prefix, WORD frameCol, WORD fillCol)
    : UIElement(x, y, width, 1), currentValue(curr), maxValue(max), prefixText(prefix), frameColor(frameCol), fillColor(fillCol) {
}

bool ProgressBarUI::Update(InputManager& input) {
    (void)input;
    return false;
}

void ProgressBarUI::Render(ScreenManager& screen) {
    if (currentValue == nullptr || maxValue == nullptr || *maxValue <= 0) return;
    if (width < 3) return;

    float ratio = static_cast<float>(*currentValue) / static_cast<float>(*maxValue);
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    const int innerWidth = width - 2;
    const int fillWidth = static_cast<int>(std::round(static_cast<float>(innerWidth) * ratio));
    const std::string infoText = prefixText + " " + std::to_string(*currentValue) + "/" + std::to_string(*maxValue);
    const std::wstring centeredInfo = TextLayout::AlignToWidth(TextLayout::Utf8ToWide(infoText), innerWidth, TextLayout::HorizontalAlign::Center);

    screen.DrawChar(x, y, '[', frameColor);
    screen.DrawChar(x + width - 1, y, ']', frameColor);

    for (int i = 0; i < innerWidth; ++i) {
        const bool isFilled = (i < fillWidth);
        screen.DrawChar(x + 1 + i, y, isFilled ? '=' : '.', isFilled ? fillColor : FOREGROUND_INTENSITY);
    }

    int cursorX = x + 1;
    for (wchar_t ch : centeredInfo) {
        const int glyphWidth = TextLayout::GetCellWidth(ch);
        if (ch != L' ') {
            screen.DrawChar(cursorX, y, ch, COLOR_WHITE);
        }
        cursorX += glyphWidth;
    }
}
