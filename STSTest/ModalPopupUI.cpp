// -----------------------------------------------------------------------------
// @file       ModalPopupUI.cpp
// -----------------------------------------------------------------------------
#include "ModalPopupUI.h"

ModalPopupUI::ModalPopupUI(int w, int h, const std::string& title)
    : UIElement(0, 0, w, h), title(title), isVisible(false) {
}

void ModalPopupUI::Open() { isVisible = true; }
void ModalPopupUI::Close() { isVisible = false; }
bool ModalPopupUI::IsVisible() const { return isVisible; }

void ModalPopupUI::SetContents(const std::vector<std::string>& textLines) {
    contents = textLines;
}

void ModalPopupUI::AddButton(const ButtonUI& button) {
    buttons.push_back(button);
}

bool ModalPopupUI::IsButtonClicked(int buttonIndex) const {
    if (buttonIndex >= 0 && buttonIndex < buttons.size()) {
        return buttons[buttonIndex].IsClicked();
    }
    return false;
}

void ModalPopupUI::CenterInScreen(int screenWidth, int screenHeight) {
    x = (screenWidth - width) / 2;
    y = (screenHeight - height) / 2;

    // 내부 버튼의 기준 좌표도 팝업에 맞춰 조정
    for (auto& btn : buttons) {
        // 임시 로직: 버튼들을 팝업 하단에 가로로 나열
        // 추후 ButtonUI 자체에 상대 좌표(Relative Position) 개념 도입 권장
    }
}

bool ModalPopupUI::Update(InputManager& input) {
    if (!isVisible) return false;

    // 내부 버튼 업데이트
    for (auto& btn : buttons) {
        btn.Update(input);
    }

    // 모달이 활성화되어 있으면, 모달 영역 밖의 마우스 클릭 이벤트도 모두 소비하여 
    // 뒷배경의 카드나 적이 클릭되지 않도록 차단함.
    return true;
}

void ModalPopupUI::Render(ScreenManager& screen) {
    if (!isVisible) return;

    // 뒷배경 덮어쓰기 (내부 지우기)
    for (int i = 0; i < height; ++i) {
        screen.DrawString(x, y + i, std::string(width, ' '), COLOR_WHITE);
    }

    // 외곽선
    screen.DrawString(x, y, "+" + std::string(width - 2, '=') + "+", COLOR_WHITE);
    screen.DrawString(x, y + height - 1, "+" + std::string(width - 2, '=') + "+", COLOR_WHITE);
    for (int i = 1; i < height - 1; ++i) {
        screen.DrawChar(x, y + i, '|', COLOR_WHITE);
        screen.DrawChar(x + width - 1, y + i, '|', COLOR_WHITE);
    }

    // 타이틀
    int titlePad = (width - (int)title.length()) / 2;
    screen.DrawString(x + titlePad, y + 2, title, COLOR_YELLOW);
    screen.DrawString(x + 2, y + 3, std::string(width - 4, '-'), COLOR_WHITE);

    // 내용
    for (size_t i = 0; i < contents.size(); ++i) {
        screen.DrawString(x + 4, y + 5 + i, contents[i], COLOR_WHITE);
    }

    // 버튼
    for (auto& btn : buttons) {
        btn.Render(screen);
    }
}