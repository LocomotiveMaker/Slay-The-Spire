// -----------------------------------------------------------------------------
// @file       EntityUI.cpp
// -----------------------------------------------------------------------------
#include "EntityUI.h"

#include "TextLayout.h"

#include <algorithm>

namespace {

bool IsBlankLike(wchar_t ch) {
    return ch == L' ' || ch == 0x2800 || ch == L'\t';
}

std::string WideToUtf8(const std::wstring& wideText) {
    if (wideText.empty()) {
        return {};
    }

    const int utf8Length = WideCharToMultiByte(
        CP_UTF8,
        0,
        wideText.data(),
        static_cast<int>(wideText.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (utf8Length <= 0) {
        return "?";
    }

    std::string utf8Text(static_cast<size_t>(utf8Length), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wideText.data(),
        static_cast<int>(wideText.size()),
        utf8Text.data(),
        utf8Length,
        nullptr,
        nullptr);
    return utf8Text;
}

std::vector<std::string> NormalizeAsciiArt(const std::vector<std::string>& lines) {
    std::vector<std::wstring> wideLines;
    wideLines.reserve(lines.size());

    int commonLeading = 100000;
    bool foundContent = false;

    for (const std::string& line : lines) {
        std::wstring wideLine = TextLayout::Utf8ToWide(line);
        int leadingCount = 0;
        int contentStart = static_cast<int>(wideLine.size());
        int contentEnd = -1;

        for (int index = 0; index < static_cast<int>(wideLine.size()); ++index) {
            if (!IsBlankLike(wideLine[static_cast<size_t>(index)])) {
                contentStart = index;
                break;
            }
            ++leadingCount;
        }

        for (int index = static_cast<int>(wideLine.size()) - 1; index >= 0; --index) {
            if (!IsBlankLike(wideLine[static_cast<size_t>(index)])) {
                contentEnd = index;
                break;
            }
        }

        if (contentEnd >= contentStart) {
            foundContent = true;
            commonLeading = (std::min)(commonLeading, leadingCount);
        }
        else {
            commonLeading = (std::min)(commonLeading, 0);
        }

        wideLines.push_back(std::move(wideLine));
    }

    if (!foundContent) {
        return {};
    }

    std::vector<std::string> normalized;
    normalized.reserve(wideLines.size());
    for (const std::wstring& wideLine : wideLines) {
        int contentStart = static_cast<int>(wideLine.size());
        int contentEnd = -1;

        for (int index = 0; index < static_cast<int>(wideLine.size()); ++index) {
            if (!IsBlankLike(wideLine[static_cast<size_t>(index)])) {
                contentStart = index;
                break;
            }
        }

        for (int index = static_cast<int>(wideLine.size()) - 1; index >= 0; --index) {
            if (!IsBlankLike(wideLine[static_cast<size_t>(index)])) {
                contentEnd = index;
                break;
            }
        }

        if (contentEnd < contentStart) {
            continue;
        }

        const int trimStart = (std::min)(contentStart, commonLeading);
        normalized.push_back(WideToUtf8(std::wstring(wideLine.begin() + trimStart, wideLine.begin() + contentEnd + 1)));
    }

    return normalized;
}

std::vector<std::string> BuildPlayerArt() {
    return NormalizeAsciiArt({
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣴⣾⣶⣄",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⣿⣿⣿⡇",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣼⣿⣿⣿⣟⣀⡀",
        u8"⣴⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡀⣼⣿⣿⣿⣿⣿⣿⣿⣿⣆",
        u8"⠛⠿⠿⠿⠿⠿⠿⠿⠿⠿⠿⠿⠿⠿⠿⠿⣿⢿⣿⣿⣿⣿⣿⣿⣿⣿⡏",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⠛⣿⣿⣿⣿⣿⢿⣿⡇",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣾⣿⣿⣿⣿⣿⡎⠛⠁",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⣿⣿⣿⣿⣿⣿⣿⣄",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⡆",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣿⣿⣿⣿⠋⠀⠀⠙⠿⣿⣿⣿⣄",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⣿⣿⡿⠋⠀⠀⠀⠀⠀⠈⢻⣿⣿⡄",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢹⣿⠁⠀⠀⠀⠀⠀⠀⠀⠈⠙⠿⣿⣦",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣤⣾⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣹⣿⡀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⠛⠋"
    });
}

std::vector<std::string> BuildBossArt() {
    return NormalizeAsciiArt({
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣀⣀⢀⣀⣀⣀⣀⡀",
        u8"⣿⣿⣿⣿⣶⠀⠀⠀⠀⠀⠀⠀⢻⣿⣿⣿⡸⣿⣿⣿⣿⣿⣶⣶⣾⣿⣿",
        u8"⣿⣿⣿⣿⡟⠀⠀⠀⠀⠀⠀⠀⠘⣿⣿⣿⣷⡈⢿⣿⣿⣿⣿⣿⣿⣿⣿",
        u8"⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀⠀⠀⠀⠘⢿⣿⣿⣿⣦⡻⣿⣿⣿⣿⣿⣿⣿",
        u8"⣿⣿⣿⣿⣧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿",
        u8"⠘⣿⣿⣿⣿⣿⣆⣴⣄⠀⠀⣀⣀⠀⠀⠀⠀⢹⣿⣿⣿⣿⣿⣿⣿⣿⣿",
        u8"⠀⠹⣿⣿⣿⣿⣿⣿⠿⢀⣾⣿⣿⣧⠀⠀⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿",
        u8"⠀⠀⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠃⠀⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⡟",
        u8"⠀⠀⠀⠘⢿⣿⣿⣿⣿⣿⣿⣿⡟⠁⠀⠀⣠⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇",
        u8"⠀⠀⠀⠀⠈⠹⢿⣿⣿⣿⣿⣿⣿⣆⢠⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇",
        u8"⠀⠀⠀⠀⠀⠀⠀⢻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿",
        u8"⠀⠀⠀⠀⠀⠀⣠⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿",
        u8"⠀⠀⠀⠀⢠⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿",
        u8"⠀⠀⠀⣰⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿",
        u8"⠀⠀⣰⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿",
        u8"⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿",
        u8"⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿",
        u8"⠀⠀⢻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿",
        u8"⠀⠀⠀⢻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿",
        u8"⠀⠀⠀⠀⠘⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠛",
        u8"⠀⠀⠀⠀⠀⢹⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠿⠋"
    });
}

std::vector<std::string> BuildEliteArt() {
    return NormalizeAsciiArt({
        u8"⠀⠀⠀⠀⣠⣶⣶⣦",
        u8"⠀⠀⣠⣾⣿⣿⣿⣿⣷⣄",
        u8"⠀⢰⣿⣿⣿⣿⣿⣿⣿⣿⡆",
        u8"⠀⠈⢿⣿⣿⣿⣿⣿⣿⡿⠁",
        u8"⠀⠀⠀⣿⣿⣿⣿⣿⣿",
        u8"⠀⠀⣰⣿⣿⣿⣿⣿⣿⣆",
        u8"⠀⣴⣿⣿⡿⠋⠀⠙⢿⣿⣦",
        u8"⠀⠿⠛⠁⠀⠀⠀⠀⠀⠈⠛⠿"
    });
}

std::vector<std::string> BuildNormalEnemyArt() {
    return NormalizeAsciiArt({
        u8"⠀⠀⢀⣀⣀⡀",
        u8"⠀⣰⣿⣿⣿⣿⣆",
        u8"⢰⣿⣿⣿⣿⣿⣿⡆",
        u8"⠘⣿⣿⣿⣿⣿⣿⠃",
        u8"⠀⠈⢿⣿⣿⡿⠁",
        u8"⠀⠀⠀⠉⠉"
    });
}

} // namespace

EntityUI::EntityUI(int x, int y, EntityData* entityData, bool playerEntity)
    : UIElement(0, 0, 24, 10),
    data(entityData),
    isPlayer(playerEntity),
    isTargeted(false),
    hitAnimationTimer(0),
    anchorCenterX(x),
    anchorBottomY(y),
    artWidth(0),
    artHeight(0),
    healthBarWidth(0),
    healthBar(nullptr) {
    if (isPlayer) {
        asciiArt = BuildPlayerArt();
    }
    else if (data != nullptr && data->id >= 9200) {
        asciiArt = BuildBossArt();
    }
    else if (data != nullptr && data->id >= 9100) {
        asciiArt = BuildEliteArt();
    }
    else {
        asciiArt = BuildNormalEnemyArt();
    }

    const WORD hpColor = isPlayer ? COLOR_GREEN : COLOR_RED;
    healthBar = new ProgressBarUI(0, 0, 20, &(data->currentHp), &(data->maxHp), u8"체력", COLOR_WHITE, hpColor);
    RefreshLayout();
}

EntityUI::~EntityUI() {
    delete healthBar;
}

void EntityUI::RefreshLayout() {
    artWidth = 0;
    artHeight = static_cast<int>(asciiArt.size());
    for (const std::string& line : asciiArt) {
        artWidth = (std::max)(artWidth, TextLayout::MeasureDisplayWidthUtf8(line));
    }

    const int nameWidth = (std::max)(artWidth, TextLayout::MeasureDisplayWidthUtf8(data ? data->name : ""));
    healthBarWidth = (std::max)(18, (std::min)(36, artWidth - 4));
    width = (std::max)(nameWidth, healthBarWidth);
    height = artHeight + 5;

    const int artTopY = GetArtTopY();
    x = anchorCenterX - (width / 2);
    y = artTopY - 2;
    healthBar->SetBarWidth(healthBarWidth);
    healthBar->SetPosition(anchorCenterX - (healthBarWidth / 2), anchorBottomY + 2);
}

void EntityUI::SetTargeted(bool state) {
    isTargeted = state;
}

void EntityUI::TriggerHitAnimation() {
    hitAnimationTimer = 15;
}

void EntityUI::SetAnchorBottomCenter(int centerX, int bottomY) {
    anchorCenterX = centerX;
    anchorBottomY = bottomY;
    RefreshLayout();
}

bool EntityUI::Update(InputManager& input) {
    if (hitAnimationTimer > 0) {
        --hitAnimationTimer;
    }

    healthBar->Update(input);
    return false;
}

void EntityUI::Render(ScreenManager& screen) {
    if (data == nullptr) {
        return;
    }

    RefreshLayout();

    int horizontalShake = 0;
    if (hitAnimationTimer > 0) {
        horizontalShake = (hitAnimationTimer % 4 < 2) ? 1 : -1;
    }

    WORD artColor = COLOR_WHITE;
    if (hitAnimationTimer > 0) {
        artColor = COLOR_RED;
    }
    else if (isTargeted) {
        artColor = COLOR_YELLOW;
    }

    const int artTopY = GetArtTopY();
    const int artLeftX = anchorCenterX - (artWidth / 2);
    for (size_t index = 0; index < asciiArt.size(); ++index) {
        const int lineWidth = TextLayout::MeasureDisplayWidthUtf8(asciiArt[index]);
        const int drawX = artLeftX + ((artWidth - lineWidth) / 2) + horizontalShake;
        screen.DrawString(drawX, artTopY + static_cast<int>(index), asciiArt[index], artColor);
    }

    const std::string headerText = isTargeted ? std::string(u8"[조준 중]") : data->name;
    screen.DrawString(
        anchorCenterX - (width / 2) + horizontalShake,
        artTopY - 2,
        TextLayout::AlignToWidth(TextLayout::Utf8ToWide(headerText), width, TextLayout::HorizontalAlign::Center),
        isTargeted ? COLOR_YELLOW : COLOR_WHITE);

    healthBar->Render(screen);
}
