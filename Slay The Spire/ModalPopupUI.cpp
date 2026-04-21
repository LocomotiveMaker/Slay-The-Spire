// -----------------------------------------------------------------------------
// @file       ModalPopupUI.cpp
// -----------------------------------------------------------------------------
#include "ModalPopupUI.h"
#include "TextLayout.h"

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
    if (buttonIndex >= 0 && buttonIndex < static_cast<int>(buttons.size())) {
        return buttons[buttonIndex].IsClicked();
    }
    return false;
}

void ModalPopupUI::CenterInScreen(int screenWidth, int screenHeight) {
    x = (screenWidth - width) / 2;
    y = (screenHeight - height) / 2;

    for (auto& btn : buttons) {
        // Buttons remain screen-anchored for now.
        (void)btn;
    }
}

bool ModalPopupUI::Update(InputManager& input) {
    if (!isVisible) return false;

    for (auto& btn : buttons) {
        btn.Update(input);
    }

    return true;
}

void ModalPopupUI::Render(ScreenManager& screen) {
    if (!isVisible) return;

    const int contentLeft = x + 4;
    const int contentWidth = width - 8;
    const int contentTop = y + 5;
    const int contentBottom = y + height - 4;

    for (int i = 0; i < height; ++i) {
        screen.DrawString(x, y + i, std::wstring(static_cast<size_t>(width), L' '), COLOR_WHITE);
    }

    screen.DrawString(x, y, "+" + std::string(width - 2, '=') + "+", COLOR_WHITE);
    screen.DrawString(x, y + height - 1, "+" + std::string(width - 2, '=') + "+", COLOR_WHITE);
    for (int i = 1; i < height - 1; ++i) {
        screen.DrawChar(x, y + i, '|', COLOR_WHITE);
        screen.DrawChar(x + width - 1, y + i, '|', COLOR_WHITE);
    }

    const std::wstring centeredTitle = TextLayout::AlignToWidth(
        TextLayout::Utf8ToWide(title),
        width - 2,
        TextLayout::HorizontalAlign::Center);

    screen.DrawString(x + 1, y + 2, centeredTitle, COLOR_YELLOW);
    screen.DrawString(x + 2, y + 3, std::string(width - 4, '-'), COLOR_WHITE);

    int drawY = contentTop;
    for (const std::string& contentLine : contents) {
        const TextLayout::WrappedText wrapped = TextLayout::WrapUtf8(contentLine, contentWidth);
        for (const std::wstring& wrappedLine : wrapped.lines) {
            if (drawY > contentBottom) {
                break;
            }

            screen.DrawString(
                contentLeft,
                drawY,
                TextLayout::AlignToWidth(wrappedLine, contentWidth, TextLayout::HorizontalAlign::Left),
                COLOR_WHITE);
            ++drawY;
        }

        if (drawY > contentBottom) {
            break;
        }
    }

    for (auto& btn : buttons) {
        btn.Render(screen);
    }
}
