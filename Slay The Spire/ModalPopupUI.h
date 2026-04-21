// -----------------------------------------------------------------------------
// @file       ModalPopupUI.h
// @brief      입력을 차단하고 화면 중앙에 배치되는 모달 팝업
// -----------------------------------------------------------------------------
#pragma once
#include "UIElement.h"
#include "ButtonUI.h"
#include <vector>
#include <string>

class ModalPopupUI : public UIElement {
private:
    std::string title;
    std::vector<std::string> contents;
    std::vector<ButtonUI> buttons;
    bool isVisible;

public:
    ModalPopupUI(int w, int h, const std::string& title);

    void Open();
    void Close();
    bool IsVisible() const;

    void SetContents(const std::vector<std::string>& textLines);
    void AddButton(const ButtonUI& button);

    // 내부 버튼의 클릭 상태를 확인하기 위한 접근자
    bool IsButtonClicked(int buttonIndex) const;

    // 모달은 화면 중앙에 강제 정렬됨
    void CenterInScreen(int screenWidth, int screenHeight);

    bool Update(InputManager& input) override;
    void Render(ScreenManager& screen) override;
};