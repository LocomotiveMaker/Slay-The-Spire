#include "CardUI.h"
#include "TextLayout.h"

namespace {

constexpr int kCardInnerWidth = 26;
constexpr int kDescriptionTop = 9;
constexpr int kDescriptionLineCount = 6;

std::wstring BuildCardLine(const std::wstring& content, TextLayout::HorizontalAlign align = TextLayout::HorizontalAlign::Left) {
    return L"|" + TextLayout::AlignToWidth(content, kCardInnerWidth, align) + L"|";
}

std::string GetCardTypeLabel(CardType type) {
    switch (type) {
    case CardType::Attack: return u8"공격";
    case CardType::Skill:  return u8"스킬";
    case CardType::Power:  return u8"파워";
    case CardType::Status: return u8"상태";
    case CardType::Curse:  return u8"저주";
    default:               return u8"미정";
    }
}

} // namespace

CardUI::CardUI(int x, int y, CardData* cardData)
    : UIElement(x, y, 28, 18), data(cardData), baseY(y) {
    RebuildLayoutCache();
}

void CardUI::RebuildLayoutCache() {
    cachedNameLine.clear();
    cachedTypeLine.clear();
    cachedDescriptionLines.clear();

    if (data == nullptr) {
        return;
    }

    cachedNameLine = BuildCardLine(TextLayout::Utf8ToWide(data->name), TextLayout::HorizontalAlign::Center);
    cachedTypeLine = BuildCardLine(TextLayout::Utf8ToWide(GetCardTypeLabel(data->type)), TextLayout::HorizontalAlign::Center);

    const TextLayout::WrappedText wrappedDescription = TextLayout::WrapUtf8(data->description, kCardInnerWidth);
    cachedDescriptionLines.reserve(static_cast<size_t>(kDescriptionLineCount));

    for (int lineIndex = 0; lineIndex < kDescriptionLineCount; ++lineIndex) {
        std::wstring descriptionLine;
        if (lineIndex < static_cast<int>(wrappedDescription.lines.size())) {
            descriptionLine = wrappedDescription.lines[lineIndex];
        }
        cachedDescriptionLines.push_back(BuildCardLine(descriptionLine));
    }
}

void CardUI::SetBasePosition(int newX, int newY) {
    SetPosition(newX, newY);
    baseY = newY;
}

bool CardUI::Update(InputManager& input) {
    bool hit = UIElement::Update(input);

    // Hovered cards float upward to preserve the existing feedback.
    y = isHovered ? baseY - 3 : baseY;

    if (isHovered && input.IsLeftClickDown()) {
        // Reserved for future card-specific interactions.
    }
    return hit;
}

void CardUI::Render(ScreenManager& screen) {
    if (data == nullptr) return;

    const WORD color = isHovered ? COLOR_YELLOW : COLOR_WHITE;

    screen.DrawString(x, y + 0, ",--------------------------.", color);
    screen.DrawString(x, y + 1, "|[" + std::to_string(data->cost) + "]                       |", color);
    screen.DrawString(x, y + 2, "|                          |", color);
    screen.DrawString(x, y + 3, cachedNameLine, color);
    screen.DrawString(x, y + 4, "|                          |", color);
    screen.DrawString(x, y + 5, "|       //========\\\\       |", color);
    screen.DrawString(x, y + 6, "|       ||  ART   ||       |", color);
    screen.DrawString(x, y + 7, "|       \\\\========//       |", color);
    screen.DrawString(x, y + 8, "|                          |", color);

    for (int lineIndex = 0; lineIndex < kDescriptionLineCount; ++lineIndex) {
        screen.DrawString(x, y + kDescriptionTop + lineIndex, cachedDescriptionLines[static_cast<size_t>(lineIndex)], color);
    }

    screen.DrawString(x, y + 15, cachedTypeLine, color);
    screen.DrawString(x, y + 16, "|                          |", color);
    screen.DrawString(x, y + 17, "`--------------------------'", color);
}
