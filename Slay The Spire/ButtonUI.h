// -----------------------------------------------------------------------------
// @file       ButtonUI.h
// @brief      재사용 가능한 범용 버튼 UI 컴포넌트
// -----------------------------------------------------------------------------
#pragma once
#include "UIElement.h"
#include <string>

class ButtonUI : public UIElement {
private:
    std::string text;
    WORD idleColor;
    WORD hoverColor;
    bool isClickedThisFrame; // 현재 프레임에서 클릭이 확정되었는지 여부

public:
    ButtonUI(int x, int y, int width, int height, const std::string& text, WORD idleColor, WORD hoverColor);

    bool Update(InputManager& input) override;
    void Render(ScreenManager& screen) override;

    // 클릭 이벤트 확인용 Getter
    bool IsClicked() const { return isClickedThisFrame; }

    // UIElement 순수 가상 함수 구현 (여기서는 콜백 대신 IsClicked 플래그 사용)
    //void OnClick() override {}
};