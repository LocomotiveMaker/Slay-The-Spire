#include "CardUI.h"
#include "TextLayout.h"

// 카드 렌더링은 폭 계산과 정렬 안정성이 중요해서, 숫자 배치와 줄바꿈을
// 여기에서 한 번에 처리한다.
namespace {

constexpr int kCardInnerWidth = 26;
constexpr int kDescriptionTop = 9;
constexpr int kDescriptionLineCount = 6;

std::wstring BuildCardLine(
    const std::wstring& content,
    TextLayout::HorizontalAlign align = TextLayout::HorizontalAlign::Left,
    int reservedRightWidth = 0) {
    const int safeReservedWidth = (std::max)(0, (std::min)(kCardInnerWidth - 1, reservedRightWidth));
    const int visibleWidth = kCardInnerWidth - safeReservedWidth;
    return L"|" +
        TextLayout::AlignToWidth(content, visibleWidth, align) +
        std::wstring(static_cast<size_t>(safeReservedWidth), L' ') +
        L"|";
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
    : UIElement(x, y, 28, 18),
    data(cardData),
    baseY(y),
    rightOcclusionChars(0),
    frameColor(COLOR_WHITE),
    playable(true) {
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

    const int hiddenDescriptionWidth = (std::max)(0, rightOcclusionChars - 1);
    const int visibleDescriptionWidth = (std::max)(1, kCardInnerWidth - hiddenDescriptionWidth);
    const TextLayout::WrappedText wrappedDescription = TextLayout::WrapUtf8(data->description, visibleDescriptionWidth);
    cachedDescriptionLines.reserve(static_cast<size_t>(kDescriptionLineCount));

    for (int lineIndex = 0; lineIndex < kDescriptionLineCount; ++lineIndex) {
        std::wstring descriptionLine;
        if (lineIndex < static_cast<int>(wrappedDescription.lines.size())) {
            descriptionLine = wrappedDescription.lines[lineIndex];
        }
        cachedDescriptionLines.push_back(BuildCardLine(descriptionLine, TextLayout::HorizontalAlign::Left, hiddenDescriptionWidth));
    }
}

void CardUI::SetBasePosition(int newX, int newY) {
    SetPosition(newX, newY);
    baseY = newY;
}

void CardUI::SetRightOcclusion(int chars) {
    const int safeChars = (std::max)(0, chars);
    if (rightOcclusionChars == safeChars) {
        return;
    }

    rightOcclusionChars = safeChars;
    RebuildLayoutCache();
}

void CardUI::SetFrameColor(WORD color) {
    frameColor = color;
}

void CardUI::SetPlayable(bool canPlay) {
    playable = canPlay;
    if (!playable) {
        isHovered = false;
    }
}

bool CardUI::Update(InputManager& input) {
    bool hit = UIElement::Update(input);

    y = (playable && isHovered) ? baseY - 3 : baseY;

    if (isHovered && input.IsLeftClickDown()) {
        // Reserved for future card-specific interactions.
    }
    return hit;
}

void CardUI::Render(ScreenManager& screen) {
    if (data == nullptr) return;

    const WORD color = playable ? (isHovered ? COLOR_YELLOW : frameColor) : FOREGROUND_INTENSITY;

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
