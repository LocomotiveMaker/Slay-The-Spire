#include "CardUI.h"

#include "CardArtLibrary.h"
#include "CardLibrary.h"
#include "TextLayout.h"

// 카드 렌더링은 폭 계산과 정렬 안정성이 중요해서, 숫자 배치와 줄바꿈을
// 여기에서 한 번에 처리한다.
namespace {

constexpr int kCardInnerWidth = 34;
constexpr int kDescriptionTop = 13;
constexpr int kDescriptionLineCount = 6;
constexpr int kArtTop = 5;
constexpr int kArtLineCount = 6;

std::string BuildHorizontalFrame(char left, char fill, char right, int width) {
    return std::string(1, left) + std::string(static_cast<size_t>((std::max)(0, width - 2)), fill) + std::string(1, right);
}

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

std::string BuildUpgradeBadge(int upgradeLevel) {
    const int safeLevel = CardLibrary::ClampUpgradeLevel(upgradeLevel);
    if (safeLevel <= 0) {
        return "";
    }

    return "+" + std::to_string(safeLevel);
}

void RedrawSideBorders(ScreenManager& screen, int x, int y, int width, WORD color) {
    screen.DrawString(x, y, "|", color);
    screen.DrawString(x + width - 1, y, "|", color);
}

} // namespace

CardUI::CardUI(int x, int y, CardData* cardData)
    : UIElement(x, y, kDefaultCardWidth, kDefaultCardHeight),
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

    y = (playable && isHovered) ? baseY - kHoverLift : baseY;

    if (isHovered && input.IsLeftClickDown()) {
        // Reserved for future card-specific interactions.
    }
    return hit;
}

void CardUI::Render(ScreenManager& screen) {
    if (data == nullptr) return;

    const WORD color = playable ? (isHovered ? COLOR_YELLOW : frameColor) : FOREGROUND_INTENSITY;
    const WORD textColor = playable ? COLOR_WHITE : FOREGROUND_INTENSITY;
    const std::vector<std::string>& artLines = CardArtLibrary::Get(*data);
    const std::string topFrame = BuildHorizontalFrame(',', '-', '.', width);
    const std::string bottomFrame = BuildHorizontalFrame('`', '-', '\'', width);

    screen.DrawString(x, y + 0, topFrame, color);
    screen.DrawString(
        x,
        y + 1,
        BuildCardLine(TextLayout::Utf8ToWide(CardLibrary::BuildCostPips(data->cost)), TextLayout::HorizontalAlign::Left),
        textColor);
    RedrawSideBorders(screen, x, y + 1, width, color);
    const std::string upgradeBadge = BuildUpgradeBadge(data->upgradeLevel);
    if (!upgradeBadge.empty()) {
        const std::wstring upgradeWide = TextLayout::Utf8ToWide(upgradeBadge);
        const int upgradeWidth = TextLayout::MeasureDisplayWidth(upgradeWide);
        const int badgeX = x + 1 + (std::max)(0, kCardInnerWidth - upgradeWidth);
        WORD upgradeColor = COLOR_GREEN;
        if (data->upgradeLevel >= 2) {
            switch ((GetTickCount() / 160) % 3) {
            case 0: upgradeColor = COLOR_RED | FOREGROUND_INTENSITY; break;
            case 1: upgradeColor = COLOR_GREEN | FOREGROUND_INTENSITY; break;
            default: upgradeColor = COLOR_YELLOW; break;
            }
        }
        screen.DrawString(badgeX, y + 1, upgradeWide, playable ? upgradeColor : FOREGROUND_INTENSITY);
    }
    screen.DrawString(x, y + 2, BuildCardLine(L""), color);
    screen.DrawString(x, y + 3, cachedNameLine, textColor);
    RedrawSideBorders(screen, x, y + 3, width, color);
    screen.DrawString(x, y + 4, BuildCardLine(L""), color);

    for (int artIndex = 0; artIndex < kArtLineCount; ++artIndex) {
        std::wstring artLine;
        if (artIndex < static_cast<int>(artLines.size())) {
            artLine = TextLayout::Utf8ToWide(artLines[static_cast<size_t>(artIndex)]);
        }
        screen.DrawString(
            x,
            y + kArtTop + artIndex,
            BuildCardLine(artLine, TextLayout::HorizontalAlign::Center),
            playable ? color : FOREGROUND_INTENSITY);
        RedrawSideBorders(screen, x, y + kArtTop + artIndex, width, color);
    }

    screen.DrawString(x, y + 11, BuildCardLine(L""), color);
    screen.DrawString(x, y + 12, BuildCardLine(L""), color);

    for (int lineIndex = 0; lineIndex < kDescriptionLineCount; ++lineIndex) {
        screen.DrawString(x, y + kDescriptionTop + lineIndex, cachedDescriptionLines[static_cast<size_t>(lineIndex)], textColor);
        RedrawSideBorders(screen, x, y + kDescriptionTop + lineIndex, width, color);
    }

    screen.DrawString(x, y + 19, BuildCardLine(L""), color);
    screen.DrawString(x, y + 20, cachedTypeLine, textColor);
    RedrawSideBorders(screen, x, y + 20, width, color);
    screen.DrawString(x, y + 21, BuildCardLine(L""), color);
    screen.DrawString(x, y + 22, BuildCardLine(L""), color);
    screen.DrawString(x, y + 23, bottomFrame, color);
}
