// -----------------------------------------------------------------------------
// @file       ButtonUI.cpp
// -----------------------------------------------------------------------------
#include "ButtonUI.h"
#include "TextLayout.h"

ButtonUI::ButtonUI(int x, int y, int width, int height, const std::string& text, WORD idleColor, WORD hoverColor)
    : UIElement(x, y, width, height), text(text), idleColor(idleColor), hoverColor(hoverColor), isClickedThisFrame(false) {
    RebuildTextCache();
}

void ButtonUI::RebuildTextCache() {
    cachedAlignedText = TextLayout::AlignToWidth(
        TextLayout::Utf8ToWide(text),
        width - 2,
        TextLayout::HorizontalAlign::Center);
}

bool ButtonUI::Update(InputManager& input) {
    isClickedThisFrame = false;
    bool hit = UIElement::Update(input);

    // 마우스 좌클릭 후 뗄 때(Release)를 클릭으로 판정할 수도 있으나,
    // 빠른 피드백을 위해 눌린 순간(Press) 처리
    // [수정] 누르고 있는 상태가 아닌, 방금 눌린 순간(Down)에만 클릭 판정
    if (isHovered && input.IsLeftClickDown()) {
        isClickedThisFrame = true;
    }

    return hit;
}

void ButtonUI::Render(ScreenManager& screen) {
    WORD color = isHovered ? hoverColor : idleColor;

    // 박스 렌더링
    screen.DrawString(x, y, "+" + std::string(width - 2, '-') + "+", color);
    for (int i = 1; i < height - 1; ++i) {
        screen.DrawString(x, y + i, "|" + std::string(width - 2, ' ') + "|", color);
    }
    screen.DrawString(x, y + height - 1, "+" + std::string(width - 2, '-') + "+", color);

    // 텍스트는 내부 폭에 맞춘 뒤 배치해야 한글이 들어와도 테두리가 밀리지 않는다.
    const int textY = TextLayout::ComputeAlignedY(y + 1, height - 2, 1);
    screen.DrawString(x + 1, textY, cachedAlignedText, color);
}
