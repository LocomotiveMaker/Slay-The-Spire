// -----------------------------------------------------------------------------
// @file       ProgressBarUI.cpp
// -----------------------------------------------------------------------------
#include "ProgressBarUI.h"
#include "TextLayout.h"

ProgressBarUI::ProgressBarUI(int x, int y, int width, int* curr, int* max, const std::string& prefix, WORD frameCol, WORD fillCol)
    : UIElement(x, y, width, 1), currentValue(curr), maxValue(max), prefixText(prefix), frameColor(frameCol), fillColor(fillCol) {
}

bool ProgressBarUI::Update(InputManager& input) {
    (void)input;
    return false;
}

void ProgressBarUI::Render(ScreenManager& screen) {
    if (currentValue == nullptr || maxValue == nullptr || *maxValue <= 0) return;

    float ratio = static_cast<float>(*currentValue) / static_cast<float>(*maxValue);
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    const int fillWidth = static_cast<int>((width - 2) * ratio);
    const std::string infoText = prefixText + " " + std::to_string(*currentValue) + "/" + std::to_string(*maxValue);
    const std::wstring centeredInfo = TextLayout::AlignToWidth(
        TextLayout::Utf8ToWide(infoText),
        width - 2,
        TextLayout::HorizontalAlign::Center);

    screen.DrawChar(x, y, '[', frameColor);
    screen.DrawChar(x + width - 1, y, ']', frameColor);

    for (int i = 1; i < width - 1; ++i) {
        if (i <= fillWidth) {
            screen.DrawChar(x + i, y, '=', fillColor);
        }
        else {
            screen.DrawChar(x + i, y, '-', FOREGROUND_INTENSITY);
        }
    }

    screen.DrawString(x + 1, y, centeredInfo, COLOR_WHITE);
}
