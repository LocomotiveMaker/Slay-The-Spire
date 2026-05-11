// -----------------------------------------------------------------------------
// @file       TooltipUI.h
// @brief      마우스 커서를 따라다니는 정보 제공용 플로팅 팝업
// -----------------------------------------------------------------------------
#pragma once
#include "UIElement.h"
#include <vector>
#include <string>

class TooltipUI : public UIElement {
private:
    std::vector<std::string> lines;
    std::vector<std::wstring> renderedLines;
    bool isVisible;

public:
    TooltipUI();

    // 출력할 텍스트를 설정하고 크기를 자동 계산함
    void SetText(const std::vector<std::string>& textLines);
    void SetVisible(bool state);
    bool IsVisible() const;

    // 마우스 위치를 기반으로 렌더링 좌표 갱신 (화면 밖으로 나가는 것 방지)
    void UpdatePosition(int mouseX, int mouseY, int screenWidth, int screenHeight);

    bool Update(InputManager& input) override;
    void Render(ScreenManager& screen) override;
};
