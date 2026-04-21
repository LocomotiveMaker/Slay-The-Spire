// -----------------------------------------------------------------------------
// @file       TooltipUI.cpp
// -----------------------------------------------------------------------------
#include "TooltipUI.h"
#include <algorithm>

TooltipUI::TooltipUI() : UIElement(0, 0, 0, 0), isVisible(false) {}

void TooltipUI::SetText(const std::vector<std::string>& textLines) {
    lines = textLines;

    // 가장 긴 문자열을 기준으로 너비 설정
    int maxWidth = 0;
    for (const auto& line : lines) {
        if ((int)line.length() > maxWidth) {
            maxWidth = (int)line.length();
        }
    }
    width = maxWidth + 4; // 좌우 여백 포함
    height = lines.size() + 2; // 상하 테두리 포함
}

void TooltipUI::SetVisible(bool state) {
    isVisible = state;
}

bool TooltipUI::IsVisible() const {
    return isVisible;
}

void TooltipUI::UpdatePosition(int mouseX, int mouseY, int screenWidth, int screenHeight) {
    // 기본적으로 마우스 우측 하단에 표시
    x = mouseX + 2;
    y = mouseY + 2;

    // 화면 우측을 벗어날 경우 마우스 좌측으로 이동
    if (x + width > screenWidth) {
        x = mouseX - width - 1;
    }
    // 화면 하단을 벗어날 경우 위로 올림
    if (y + height > screenHeight) {
        y = screenHeight - height;
    }
}

bool TooltipUI::Update(InputManager& input) {
    return false; // 툴팁은 입력을 소비하지 않음
}

void TooltipUI::Render(ScreenManager& screen) {
    if (!isVisible) return;

    // 배경을 공백으로 지우기 (투명화 방지)
    for (int i = 0; i < height; ++i) {
        screen.DrawString(x, y + i, std::string(width, ' '), COLOR_WHITE);
    }

    // 테두리
    screen.DrawString(x, y, "+" + std::string(width - 2, '-') + "+", COLOR_WHITE);
    screen.DrawString(x, y + height - 1, "+" + std::string(width - 2, '-') + "+", COLOR_WHITE);
    for (int i = 1; i < height - 1; ++i) {
        screen.DrawChar(x, y + i, '|', COLOR_WHITE);
        screen.DrawChar(x + width - 1, y + i, '|', COLOR_WHITE);
    }

    // 텍스트 출력
    for (size_t i = 0; i < lines.size(); ++i) {
        screen.DrawString(x + 2, y + 1 + i, lines[i], COLOR_YELLOW);
    }
}