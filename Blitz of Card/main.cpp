// -----------------------------------------------------------------------------
// @file       main.cpp
// @brief      타이틀, 런, 전투, 오버레이를 묶는 메인 루프
// -----------------------------------------------------------------------------
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "AudioManager.h"
#include "AsciiArtLibrary.h"
#include "ButtonUI.h"
#include "CardArtLibrary.h"
#include "CardUI.h"
#include "CardLibrary.h"
#include "CombatSystem.h"
#include "EntityUI.h"
#include "InputManager.h"
#include "MapRenderer.h"
#include "RunState.h"
#include "SaveManager.h"
#include "ScreenManager.h"
#include "TargetingArrow.h"
#include "TextLayout.h"
#include "TooltipUI.h"

using namespace std;

namespace {

// 메인 파일 로컬 유틸리티 모음.
struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool Contains(int px, int py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }
};

void RenderFrameBox(ScreenManager& screen, const Rect& rect, WORD color);
void RenderWrappedText(ScreenManager& screen, int x, int y, int width, const string& text, WORD color);

enum class EndingStage {
    None,
    DefeatReveal,
    DefeatRecord,
    VictoryReveal,
    VictorySummary
};

enum class SpeechTailStyle {
    None,
    BottomRight,
    TopRight
};

int GetTargetRefreshRate() {
    HDC desktopDc = GetDC(nullptr);
    if (desktopDc == nullptr) {
        return 144;
    }

    const int refreshRate = GetDeviceCaps(desktopDc, VREFRESH);
    ReleaseDC(nullptr, desktopDc);

    if (refreshRate <= 1) {
        return 144;
    }

    return (std::min)(240, refreshRate);
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

string FormatFloat(float value, int precision = 1) {
    char buffer[64] = {};
    sprintf_s(buffer, "%.*f", precision, value);
    return buffer;
}

string FormatPlayTime(int totalSeconds) {
    const int safeSeconds = (std::max)(0, totalSeconds);
    const int minutes = safeSeconds / 60;
    const int seconds = safeSeconds % 60;
    char buffer[32] = {};
    sprintf_s(buffer, "%02d:%02d", minutes, seconds);
    return buffer;
}

bool ConsumeKeyPress(int virtualKey, bool& previousState) {
    const bool currentState = (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
    const bool pressedThisFrame = currentState && !previousState;
    previousState = currentState;
    return pressedThisFrame;
}

uint32_t BuildSeed(const SettingsData& settings) {
    if (!settings.customSeedText.empty()) {
        return static_cast<uint32_t>(hash<string>{}(settings.customSeedText));
    }

    const uint64_t timeSeed = static_cast<uint64_t>(chrono::steady_clock::now().time_since_epoch().count());
    random_device rd;
    return static_cast<uint32_t>((timeSeed ^ rd()) & 0xFFFFFFFFu);
}

float Clamp01(float value) {
    return (std::max)(0.0f, (std::min)(1.0f, value));
}

float EaseInOutCubic(float value) {
    const float t = Clamp01(value);
    return (t < 0.5f)
        ? (4.0f * t * t * t)
        : (1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f);
}

float EaseOutCubic(float value) {
    const float t = Clamp01(value);
    return 1.0f - std::pow(1.0f - t, 3.0f);
}

int LerpInt(int from, int to, float t) {
    return static_cast<int>(std::round(static_cast<float>(from) + ((static_cast<float>(to - from)) * Clamp01(t))));
}

Rect LerpRectFixedTopCenter(const Rect& from, const Rect& to, float t) {
    const float eased = Clamp01(t);
    const int width = LerpInt(from.width, to.width, eased);
    const int height = LerpInt(from.height, to.height, eased);
    const int centerX = from.x + (from.width / 2);

    Rect result = {};
    result.width = width;
    result.height = height;
    result.x = centerX - (width / 2);
    result.y = from.y;
    return result;
}

Rect LerpRectFixedBottomRight(const Rect& from, const Rect& to, float t) {
    const float eased = Clamp01(t);
    const int width = LerpInt(from.width, to.width, eased);
    const int height = LerpInt(from.height, to.height, eased);
    const int right = from.x + from.width;
    const int bottom = from.y + from.height;

    Rect result = {};
    result.width = width;
    result.height = height;
    result.x = right - width;
    result.y = bottom - height;
    return result;
}

Rect LerpRectFixedBottomLeft(const Rect& from, const Rect& to, float t) {
    const float eased = Clamp01(t);
    const int width = LerpInt(from.width, to.width, eased);
    const int height = LerpInt(from.height, to.height, eased);
    const int bottom = from.y + from.height;

    Rect result = {};
    result.width = width;
    result.height = height;
    result.x = from.x;
    result.y = bottom - height;
    return result;
}

Rect LerpRectFixedBottomCenter(const Rect& from, const Rect& to, float t) {
    const float eased = Clamp01(t);
    const int width = LerpInt(from.width, to.width, eased);
    const int height = LerpInt(from.height, to.height, eased);
    const int centerX = from.x + (from.width / 2);
    const int bottom = from.y + from.height;

    Rect result = {};
    result.width = width;
    result.height = height;
    result.x = centerX - (width / 2);
    result.y = bottom - height;
    return result;
}

constexpr int kNeowCardPackOffsetX = -40;
constexpr int kCardPackExpandedPanelSideMargin = 3;
constexpr int kCardPackExpandedPanelTopGap = 2;
constexpr int kTitleSettingsSliderWidth = 32;
constexpr int kRunSettingsSliderWidth = 50;
constexpr int kCombatEnergyPanelX = 0;
constexpr int kCombatHandRaiseRows = 3;
constexpr float kPowerBannerDurationSec = 1.0f;

struct ArtPreviewEntry {
    AsciiArtId id = AsciiArtId::TitleLogo;
    std::string name;
    WORD color = COLOR_WHITE;
};

const std::vector<ArtPreviewEntry>& GetArtPreviewEntries() {
    static const std::vector<ArtPreviewEntry> kEntries = {
        { AsciiArtId::TitleLogo, u8"타이틀 로고 (@)", COLOR_WHITE },
        { AsciiArtId::PlayerBattle, u8"플레이어 전투", COLOR_WHITE },
        { AsciiArtId::PlayerDeath, u8"플레이어 사망", COLOR_RED },
        { AsciiArtId::PlayerCardPack, u8"카드팩 플레이어", COLOR_WHITE },
        { AsciiArtId::EnemyNormal, u8"적 기본", COLOR_WHITE },
        { AsciiArtId::EnemyElite, u8"적 엘리트", COLOR_YELLOW },
        { AsciiArtId::EnemyBoss, u8"적 보스", COLOR_RED },
        { AsciiArtId::Neow, u8"니오우", COLOR_WHITE },
        { AsciiArtId::Merchant, u8"상인", COLOR_WHITE },
        { AsciiArtId::TreasureChestClosed, u8"보물 상자(닫힘)", COLOR_YELLOW },
        { AsciiArtId::TreasureChestOpen, u8"보물 상자(열림)", COLOR_YELLOW },
        { AsciiArtId::Campfire, u8"모닥불", COLOR_RED | COLOR_GREEN | FOREGROUND_INTENSITY }
    };
    return kEntries;
}

std::vector<int> BuildStarterPackOfferIndices(std::uint32_t seed, int packCount, int offerCount = 3) {
    std::vector<int> indices;
    indices.reserve(static_cast<size_t>(packCount));
    for (int index = 0; index < packCount; ++index) {
        indices.push_back(index);
    }

    std::mt19937 rng(seed ^ 0x51ED270Bu);
    std::shuffle(indices.begin(), indices.end(), rng);
    if (static_cast<int>(indices.size()) > offerCount) {
        indices.resize(static_cast<size_t>(offerCount));
    }
    return indices;
}

WORD GetCardTypeFrameColor(CardType type) {
    switch (type) {
    case CardType::Attack:
        return COLOR_RED;
    case CardType::Skill:
        return COLOR_BLUE;
    case CardType::Power:
        return FOREGROUND_BLUE;
    default:
        return COLOR_WHITE;
    }
}

bool IsGroundUsableCard(const CardData& card) {
    return card.type == CardType::Skill || card.type == CardType::Power;
}

std::wstring BuildRotatingAudioAlias(const std::wstring& aliasBase, int& aliasCounter) {
    const int slot = aliasCounter++ % 16;
    return aliasBase + L"_" + std::to_wstring(slot);
}

const std::wstring& PickRandomAudio(const std::vector<std::wstring>& pool, std::mt19937& rng) {
    static const std::wstring kEmpty;
    if (pool.empty()) {
        return kEmpty;
    }
    if (pool.size() == 1) {
        return pool.front();
    }

    std::uniform_int_distribution<size_t> dist(0, pool.size() - 1);
    return pool[dist(rng)];
}

void PlayRandomEffect(AudioManager& audio, const std::vector<std::wstring>& pool, const std::wstring& aliasBase, std::mt19937& rng, int& aliasCounter, bool enabled, bool loop = false) {
    if (!enabled || pool.empty()) {
        return;
    }
    audio.PlayEffect(PickRandomAudio(pool, rng), loop ? aliasBase : BuildRotatingAudioAlias(aliasBase, aliasCounter), loop);
}

void PlayFixedEffect(AudioManager& audio, const std::wstring& filename, const std::wstring& aliasBase, int& aliasCounter, bool enabled, bool loop = false) {
    if (!enabled || filename.empty()) {
        return;
    }
    audio.PlayEffect(filename, loop ? aliasBase : BuildRotatingAudioAlias(aliasBase, aliasCounter), loop);
}

void StopLoopEffect(AudioManager& audio, const std::wstring& aliasBase) {
    audio.StopEffect(aliasBase);
}

void UpdateButtonAudio(
    const ButtonUI& button,
    const std::string& key,
    AudioManager& audio,
    const std::vector<std::wstring>& hoverPool,
    const std::vector<std::wstring>& clickPool,
    std::unordered_map<std::string, bool>& hoverLatch,
    std::mt19937& rng,
    int& aliasCounter,
    bool enabled) {
    const bool hovered = button.IsHovered();
    const bool wasHovered = hoverLatch[key];
    if (hovered && !wasHovered) {
        PlayRandomEffect(audio, hoverPool, L"sfx_ui_hover", rng, aliasCounter, enabled);
    }
    if (button.IsClicked()) {
        PlayRandomEffect(audio, clickPool, L"sfx_ui_click", rng, aliasCounter, enabled);
    }
    hoverLatch[key] = hovered;
}

void UpdateRectHoverAudio(
    bool hovered,
    const std::string& key,
    AudioManager& audio,
    const std::vector<std::wstring>& hoverPool,
    std::unordered_map<std::string, bool>& hoverLatch,
    std::mt19937& rng,
    int& aliasCounter,
    bool enabled) {
    const bool wasHovered = hoverLatch[key];
    if (hovered && !wasHovered) {
        PlayRandomEffect(audio, hoverPool, L"sfx_hover_misc", rng, aliasCounter, enabled);
    }
    hoverLatch[key] = hovered;
}

bool IsHeavyAttackCard(const CardData& card) {
    switch (card.id) {
    case CardLibrary::Id::ComboFinisher:
    case CardLibrary::Id::WhirlBeat:
    case CardLibrary::Id::HeavyStrike:
    case CardLibrary::Id::GiantArm:
    case CardLibrary::Id::WallCollapse:
    case CardLibrary::Id::ShieldSpin:
        return true;
    default:
        return card.cost >= 3;
    }
}

bool IsSpecialAttackCard(const CardData& card) {
    switch (card.id) {
    case CardLibrary::Id::RepeatingStab:
    case CardLibrary::Id::FrenzyCuts:
    case CardLibrary::Id::BouncingBlade:
    case CardLibrary::Id::ShredBurst:
        return true;
    default:
        return false;
    }
}

bool IsFireAttackCard(const CardData& card) {
    return card.id == CardLibrary::Id::BloodAccelerate;
}

bool IsThunderCard(const CardData& card) {
    return card.id == CardLibrary::Id::HeavyStrike || card.id == CardLibrary::Id::StrengthTraining;
}

std::vector<std::string> GetBigEnergyRows(int energy, int maxEnergy) {
    const std::string valueText = std::to_string((std::max)(0, energy)) + "/" + std::to_string((std::max)(0, maxEnergy));
    const char* digits[11][5] = {
        { u8"███", u8"█ █", u8"█ █", u8"█ █", u8"███" },
        { u8" ██", u8"███", u8" ██", u8" ██", u8"███" },
        { u8"███", u8"  █", u8"███", u8"█  ", u8"███" },
        { u8"███", u8"  █", u8"███", u8"  █", u8"███" },
        { u8"█ █", u8"█ █", u8"███", u8"  █", u8"  █" },
        { u8"███", u8"█  ", u8"███", u8"  █", u8"███" },
        { u8"███", u8"█  ", u8"███", u8"█ █", u8"███" },
        { u8"███", u8"  █", u8"  █", u8" █ ", u8" █ " },
        { u8"███", u8"█ █", u8"███", u8"█ █", u8"███" },
        { u8"███", u8"█ █", u8"███", u8"  █", u8"███" },
        { u8"  █", u8" ██", u8" ██", u8"██ ", u8"█  " }
    };

    std::vector<std::string> rows(5);
    for (char ch : valueText) {
        const int glyphIndex = (ch == '/') ? 10 : (ch >= '0' && ch <= '9' ? ch - '0' : -1);
        if (glyphIndex < 0) {
            continue;
        }
        for (int row = 0; row < 5; ++row) {
            if (!rows[static_cast<size_t>(row)].empty()) {
                rows[static_cast<size_t>(row)] += " ";
            }
            rows[static_cast<size_t>(row)] += digits[glyphIndex][row];
        }
    }
    return rows;
}

std::string BuildShortPowerBannerText(const std::string& source, int maxChars = 3) {
    if (source.empty()) {
        return {};
    }

    const std::wstring wide = TextLayout::Utf8ToWide(source);
    if (wide.empty()) {
        return source;
    }

    const int clampedCount = (std::max)(1, (std::min)(maxChars, static_cast<int>(wide.size())));
    return WideToUtf8(std::wstring(wide.begin(), wide.begin() + clampedCount));
}

void RenderBigEnergy(ScreenManager& screen, const Rect& rect, int energy, int maxEnergy, WORD color) {
    RenderFrameBox(screen, rect, color);
    screen.DrawString(rect.x + 2, rect.y + 1, u8"에너지", color);

    const std::vector<std::string> rows = GetBigEnergyRows(energy, maxEnergy);
    const int startY = rect.y + 3;
    for (int row = 0; row < static_cast<int>(rows.size()); ++row) {
        screen.DrawString(rect.x + 2, startY + row, rows[static_cast<size_t>(row)], color);
    }
}

std::string BuildCompactCardTypeLabel(CardType type) {
    switch (type) {
    case CardType::Attack: return u8"공격";
    case CardType::Skill: return u8"스킬";
    case CardType::Power: return u8"파워";
    default: return u8"기타";
    }
}

void RenderSpeechBubble(ScreenManager& screen, const Rect& rect, const std::string& text, WORD color, SpeechTailStyle tailStyle) {
    RenderFrameBox(screen, rect, color);
    RenderWrappedText(screen, rect.x + 2, rect.y + 1, rect.width - 4, text, color);

    switch (tailStyle) {
    case SpeechTailStyle::BottomRight:
        screen.DrawString(rect.x + rect.width - 4, rect.y + rect.height - 1, "\\", color);
        screen.DrawString(rect.x + rect.width - 3, rect.y + rect.height, " \\", color);
        break;
    case SpeechTailStyle::TopRight:
        screen.DrawString(rect.x + rect.width - 5, rect.y - 1, "/\\", color);
        screen.DrawString(rect.x + rect.width - 4, rect.y - 2, "|", color);
        break;
    default:
        break;
    }
}

void RenderCompactCardOffer(ScreenManager& screen, const Rect& rect, const CardData& card, int price, bool sold, bool hovered) {
    const WORD accentColor = sold ? FOREGROUND_INTENSITY : (hovered ? COLOR_YELLOW : GetCardTypeFrameColor(card.type));
    RenderFrameBox(screen, rect, accentColor);
    screen.DrawString(rect.x + 1, rect.y + 1, TextLayout::AlignToWidth(TextLayout::Utf8ToWide("[" + to_string(card.cost) + "]"), rect.width - 2, TextLayout::HorizontalAlign::Left), accentColor);
    screen.DrawString(rect.x + 1, rect.y + 2, TextLayout::AlignToWidth(TextLayout::Utf8ToWide(card.name), rect.width - 2, TextLayout::HorizontalAlign::Center), sold ? FOREGROUND_INTENSITY : COLOR_WHITE);

    const vector<string>& artLines = CardArtLibrary::Get(card);
    for (int artIndex = 0; artIndex < 5 && artIndex < static_cast<int>(artLines.size()) && (rect.y + 3 + artIndex) < (rect.y + rect.height - 3); ++artIndex) {
        const std::wstring artLine = TextLayout::Utf8ToWide(artLines[static_cast<size_t>(artIndex)]);
        screen.DrawString(rect.x + 1, rect.y + 3 + artIndex, TextLayout::AlignToWidth(artLine, rect.width - 2, TextLayout::HorizontalAlign::Center), sold ? FOREGROUND_INTENSITY : accentColor);
    }

    const int footerY = rect.y + rect.height - 3;
    screen.DrawString(rect.x + 1, footerY, TextLayout::AlignToWidth(TextLayout::Utf8ToWide(BuildCompactCardTypeLabel(card.type)), rect.width - 2, TextLayout::HorizontalAlign::Center), FOREGROUND_INTENSITY);
    screen.DrawString(rect.x + 1, rect.y + rect.height - 2, TextLayout::AlignToWidth(TextLayout::Utf8ToWide(sold ? u8"구매 완료" : (to_string(price) + u8"G")), rect.width - 2, TextLayout::HorizontalAlign::Center), sold ? FOREGROUND_INTENSITY : COLOR_YELLOW);
}

const std::vector<std::vector<std::string>>& GetCampfireAnimFrames() {
    static const std::vector<std::vector<std::string>> kFrames = {
        {
            u8"          (  )    ( )",
            u8"         (   )   (   )",
            u8"          \\  /\\  //",
            u8"           \\/  \\//",
            u8"        ____\\  //____",
            u8"       /_____/\\\\_____\\",
            u8"            /__\\\\"
        },
        {
            u8"           ( )    (  )",
            u8"         (  ** ) ( ** )",
            u8"           \\/ /\\ \\\\",
            u8"            /_/  \\_\\",
            u8"        ____\\    //____",
            u8"       /_____/\\\\_____\\",
            u8"            /__\\\\"
        },
        {
            u8"          ( ** )  ( )",
            u8"         (   )   ( ** )",
            u8"            \\ /\\ //",
            u8"             \\  //",
            u8"        ____//||\\\\____",
            u8"       /_____/\\\\_____\\",
            u8"            /__\\\\"
        },
        {
            u8"           (  )   ( )",
            u8"         ( ** ) (   )",
            u8"           //\\  /\\\\",
            u8"          //  \\/  \\\\",
            u8"        ____\\\\  //____",
            u8"       /_____/\\\\_____\\",
            u8"            /__\\\\"
        }
    };
    return kFrames;
}

int MeasureArtWidth(const std::vector<std::string>& lines) {
    int width = 0;
    for (const std::string& line : lines) {
        width = (std::max)(width, TextLayout::MeasureDisplayWidthUtf8(line));
    }
    return width;
}

void RenderAnchoredArt(ScreenManager& screen, int centerX, int bottomY, const std::vector<std::string>& lines, WORD color) {
    if (lines.empty()) {
        return;
    }

    const int artWidth = MeasureArtWidth(lines);
    const int artTopY = bottomY - static_cast<int>(lines.size()) + 1;
    const int artLeftX = centerX - (artWidth / 2);
    for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const int lineWidth = TextLayout::MeasureDisplayWidthUtf8(lines[lineIndex]);
        const int drawX = artLeftX + ((artWidth - lineWidth) / 2);
        screen.DrawString(drawX, artTopY + static_cast<int>(lineIndex), lines[lineIndex], color);
    }
}

int ComputeAnchoredBottomYFromCenterY(int centerY, int artHeight) {
    const int safeHeight = (std::max)(1, artHeight);
    return centerY + ((safeHeight - 1) / 2);
}

void RenderCenteredArt(ScreenManager& screen, int centerX, int centerY, const std::vector<std::string>& lines, WORD color) {
    RenderAnchoredArt(
        screen,
        centerX,
        ComputeAnchoredBottomYFromCenterY(centerY, static_cast<int>(lines.size())),
        lines,
        color);
}

void RenderAnchoredArtClipped(ScreenManager& screen, int centerX, int bottomY, const std::vector<std::string>& lines, WORD color, const Rect& clipRect) {
    if (lines.empty()) {
        return;
    }

    const int artWidth = MeasureArtWidth(lines);
    const int artTopY = bottomY - static_cast<int>(lines.size()) + 1;
    const int artLeftX = centerX - (artWidth / 2);

    for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const int drawY = artTopY + static_cast<int>(lineIndex);
        if (drawY < clipRect.y || drawY >= clipRect.y + clipRect.height) {
            continue;
        }

        const std::wstring wideLine = TextLayout::Utf8ToWide(lines[lineIndex]);
        const int lineWidth = TextLayout::MeasureDisplayWidth(wideLine);
        int cursorX = artLeftX + ((artWidth - lineWidth) / 2);

        for (wchar_t ch : wideLine) {
            const int cellWidth = TextLayout::GetCellWidth(ch);
            const bool insideClip =
                cursorX >= clipRect.x &&
                cursorX + cellWidth <= clipRect.x + clipRect.width;
            if (insideClip) {
                screen.DrawChar(cursorX, drawY, ch, color);
            }
            cursorX += cellWidth;
        }
    }
}

void RenderCenteredArtClipped(ScreenManager& screen, int centerX, int centerY, const std::vector<std::string>& lines, WORD color, const Rect& clipRect) {
    RenderAnchoredArtClipped(
        screen,
        centerX,
        ComputeAnchoredBottomYFromCenterY(centerY, static_cast<int>(lines.size())),
        lines,
        color,
        clipRect);
}

std::vector<std::string> BuildStatusTooltipLines(const std::string& key, int value) {
    if (key == u8"방어도") {
        return { string(u8"방어도 ") + std::to_string(value), u8"먼저 소모되는 임시 보호막입니다." };
    }
    if (key == u8"힘") {
        return { string(u8"힘 ") + std::to_string(value), u8"공격 피해가 그 수치만큼 증가합니다." };
    }
    if (key == u8"민첩") {
        return { string(u8"민첩 ") + std::to_string(value), u8"방어를 얻는 카드와 효과의 수치가 증가합니다." };
    }
    if (key == u8"취약") {
        return { string(u8"취약 ") + std::to_string(value), u8"받는 공격 피해가 50% 증가합니다." };
    }
    if (key == u8"약화") {
        return { string(u8"약화 ") + std::to_string(value), u8"주는 공격 피해가 25% 감소합니다." };
    }
    if (key == u8"독") {
        return { string(u8"독 ") + std::to_string(value), u8"행위가 끝날 때 독 수치만큼 피해를 받고 1 감소합니다." };
    }
    return { key + " " + std::to_string(value) };
}

std::vector<std::string> BuildRelicTooltipLines(const RelicData& relic) {
    if (relic.name.empty() && relic.description.empty()) {
        return {};
    }
    return { relic.name, relic.description };
}

bool RectFitsInside(const Rect& outer, const Rect& inner) {
    return inner.x >= outer.x &&
        inner.y >= outer.y &&
        inner.x + inner.width <= outer.x + outer.width &&
        inner.y + inner.height <= outer.y + outer.height;
}

void RenderFrameBox(ScreenManager& screen, const Rect& rect, WORD color) {
    if (rect.width < 2 || rect.height < 2) {
        return;
    }

    screen.DrawString(rect.x, rect.y, "+" + string(rect.width - 2, '-') + "+", color);
    for (int row = 1; row < rect.height - 1; ++row) {
        screen.DrawString(rect.x, rect.y + row, "|" + string(rect.width - 2, ' ') + "|", color);
    }
    screen.DrawString(rect.x, rect.y + rect.height - 1, "+" + string(rect.width - 2, '-') + "+", color);
}

void RenderPanelTitle(ScreenManager& screen, const Rect& rect, const string& title, WORD color) {
    const int innerWidth = rect.width - 2;
    screen.DrawString(
        rect.x + 1,
        rect.y + 1,
        TextLayout::AlignToWidth(TextLayout::Utf8ToWide(title), innerWidth, TextLayout::HorizontalAlign::Center),
        color);
}

void RenderWrappedText(ScreenManager& screen, int x, int y, int width, const string& text, WORD color) {
    const TextLayout::WrappedText wrapped = TextLayout::WrapUtf8(text, width);
    for (size_t index = 0; index < wrapped.lines.size(); ++index) {
        screen.DrawString(x, y + static_cast<int>(index), wrapped.lines[index], color);
    }
}

void RenderSlider(ScreenManager& screen, int x, int y, int width, const string& label, int value, bool active) {
    const int gaugeWidth = (std::max)(8, width);
    const string gaugeInner(static_cast<size_t>(gaugeWidth), '.');
    string gauge = gaugeInner;
    const int fillCount = static_cast<int>(std::round((static_cast<float>(gauge.size()) * value) / 100.0f));
    for (int index = 0; index < fillCount && index < static_cast<int>(gauge.size()); ++index) {
        gauge[static_cast<size_t>(index)] = '#';
    }

    const WORD color = active ? COLOR_YELLOW : COLOR_WHITE;
    screen.DrawString(x, y, label, color);
    screen.DrawString(x + 14, y, "[" + gauge + "]", color);
    screen.DrawString(x + 17 + gaugeWidth, y, to_string(value), color);
}

int GetSliderValueFromMouse(const Rect& trackRect, int mouseX) {
    if (trackRect.width <= 1) {
        return 0;
    }

    const int clampedX = (std::max)(trackRect.x, (std::min)(trackRect.x + trackRect.width - 1, mouseX));
    const float t = static_cast<float>(clampedX - trackRect.x) / static_cast<float>(trackRect.width - 1);
    return static_cast<int>(std::round(t * 100.0f));
}

string BuildPotionListText(const vector<PotionData>& potions) {
    if (potions.empty()) {
        return u8"없음";
    }

    string text;
    for (size_t index = 0; index < potions.size(); ++index) {
        if (index > 0) {
            text += ", ";
        }
        text += potions[index].name;
    }
    return text;
}

string BuildRelicListText(const vector<RelicData>& relics) {
    if (relics.empty()) {
        return u8"없음";
    }

    string text;
    for (size_t index = 0; index < relics.size(); ++index) {
        if (index > 0) {
            text += ", ";
        }
        text += relics[index].name;
    }
    return text;
}

string BuildNodeVisitedText(const vector<RunNodeType>& visitedNodes) {
    if (visitedNodes.empty()) {
        return u8"기록 없음";
    }

    string text;
    for (size_t index = 0; index < visitedNodes.size(); ++index) {
        if (index > 0) {
            text += " > ";
        }
        text += RunNodeTypeToDisplayName(visitedNodes[index]);
    }
    return text;
}

string BuildEventChoiceButtonText(const EventChoiceState& choice) {
    string text = "[" + choice.label + "]";
    if (!choice.previewText.empty() &&
        choice.previewText != u8"아무 일도 일어나지 않습니다." &&
        choice.previewText != u8"아무 효과도 없습니다.") {
        text += " " + choice.previewText;
    }
    return text;
}

void UpdateSliderDrag(
    InputManager& input,
    int mouseX,
    int sliderId,
    const Rect& trackRect,
    int& value,
    int& activeSliderId) {
    if (input.IsLeftClickDown() && trackRect.Contains(mouseX, input.GetMouseY())) {
        activeSliderId = sliderId;
    }

    if (activeSliderId == sliderId && input.IsLeftClick()) {
        value = GetSliderValueFromMouse(trackRect, mouseX);
    }

    if (!input.IsLeftClick() && activeSliderId == sliderId) {
        activeSliderId = -1;
    }
}

vector<string> BuildDeckLines(const RunStateData& run) {
    vector<string> lines;
    lines.reserve(run.deck.size());
    for (const CardData& card : run.deck) {
        string line = "[" + to_string(card.cost) + "] " + card.name;
        if (card.upgradeLevel > 0) {
            line += " +" + to_string(card.upgradeLevel);
        }
        line += " / " + card.description;
        lines.push_back(line);
    }
    return lines;
}

vector<string> BuildStatsLines(const GlobalStatsData& stats) {
    return {
        string(u8"총 승리 수: ") + to_string(stats.totalWins),
        string(u8"총 패배 수: ") + to_string(stats.totalLosses),
        string(u8"총 처치 적 수: ") + to_string(stats.totalEnemiesDefeated),
        string(u8"총 사용 카드 수: ") + to_string(stats.totalCardsUsed),
        string(u8"총 버린 카드 수: ") + to_string(stats.totalCardsDiscarded),
        string(u8"총 상승 층수: ") + to_string(stats.totalFloorsClimbed),
        string(u8"누적 플레이 시간: ") + FormatPlayTime(stats.totalPlayTimeSec)
    };
}

string BuildCardTypeText(CardType type) {
    switch (type) {
    case CardType::Attack: return u8"공격";
    case CardType::Skill:  return u8"스킬";
    case CardType::Power:  return u8"파워";
    case CardType::Status: return u8"상태";
    case CardType::Curse:  return u8"저주";
    default:               return u8"카드";
    }
}

string BuildCardSummary(const CardData& card) {
    string text = "[" + to_string(card.cost) + "] " + card.name;
    if (card.upgradeLevel > 0) {
        text += " +" + to_string(card.upgradeLevel);
    }
    text += " / " + BuildCardTypeText(card.type);
    return text;
}

bool HasPotionSlot(const RunStateData& run) {
    return run.potions.size() < 3;
}

bool IsBattleNodeType(RunNodeType type) {
    return type == RunNodeType::Battle || type == RunNodeType::Elite || type == RunNodeType::Boss;
}

CardData MakeRewardCard(
    int id,
    const string& name,
    int cost,
    const string& description,
    CardType type,
    CardTargetType targetType,
    CardEffectType effectType,
    CardDiscardEffectType discardEffectType,
    int primaryValue,
    int secondaryValue) {
    CardData card = {};
    card.id = id;
    card.name = name;
    card.cost = cost;
    card.description = description;
    card.type = type;
    card.targetType = targetType;
    card.effectType = effectType;
    card.discardEffectType = discardEffectType;
    card.primaryValue = primaryValue;
    card.secondaryValue = secondaryValue;
    return card;
}

PotionData MakeRewardPotion(int id, const string& name, const string& description, bool battleOnly) {
    PotionData potion = {};
    potion.id = id;
    potion.name = name;
    potion.description = description;
    potion.battleOnly = battleOnly;
    return potion;
}

int GetRewardRarityWeight(CardRarity rarity) {
    switch (rarity) {
    case CardRarity::Common:
        return 70;
    case CardRarity::Uncommon:
        return 22;
    case CardRarity::Rare:
        return 8;
    default:
        return 1;
    }
}

bool HasCardChoiceId(const std::vector<CardData>& cards, int id) {
    return std::any_of(cards.begin(), cards.end(), [&](const CardData& card) {
        return card.id == id;
    });
}

CardData PickWeightedUniqueRewardCard(
    std::mt19937& rng,
    const std::vector<CardData>& pool,
    const std::vector<CardData>& chosen) {
    std::vector<int> candidateIndices;
    std::vector<int> weights;
    for (int index = 0; index < static_cast<int>(pool.size()); ++index) {
        const CardData& card = pool[static_cast<size_t>(index)];
        if (HasCardChoiceId(chosen, card.id)) {
            continue;
        }
        candidateIndices.push_back(index);
        weights.push_back(GetRewardRarityWeight(card.rarity));
    }

    if (candidateIndices.empty()) {
        return CardData{};
    }

    std::discrete_distribution<int> pickDist(weights.begin(), weights.end());
    const int pickedSlot = candidateIndices[static_cast<size_t>(pickDist(rng))];
    return pool[static_cast<size_t>(pickedSlot)];
}

vector<CardData> BuildBattleRewardCardPool() {
    return CardLibrary::BuildGeneralCardPool();
}

vector<PotionData> BuildBattleRewardPotionPool() {
    return {
        MakeRewardPotion(8100, u8"회복 포션", u8"체력을 소량 회복합니다.", false),
        MakeRewardPotion(8101, u8"에너지 포션", u8"즉시 에너지를 회복합니다.", false),
        MakeRewardPotion(8102, u8"연막 포션", u8"보스전이 아닌 전투에서 도주합니다.", true)
    };
}

BattleRewardState BuildBattleRewardState(const RunStateData& run, RunNodeType roomType, RunNodeResultType result) {
    BattleRewardState reward = {};
    reward.active = true;

    if (result == RunNodeResultType::Escape) {
        reward.title = u8"전리품";
        reward.message = u8"도망쳤다...";
        return reward;
    }

    std::mt19937 rng(run.seed ^ static_cast<uint32_t>((run.currentNodeId + 1) * 2654435761u) ^ 0x9E3779B9u);
    const vector<CardData> cardPool = BuildBattleRewardCardPool();
    const vector<PotionData> potionPool = BuildBattleRewardPotionPool();

    reward.title = u8"전리품";
    reward.message = roomType == RunNodeType::Elite
        ? u8"엘리트 전투를 정리했습니다. 남은 보상을 챙기세요."
        : u8"전투가 끝났습니다. 남은 보상을 챙기세요.";

    std::uniform_int_distribution<int> goldDist(roomType == RunNodeType::Elite ? 35 : 20, roomType == RunNodeType::Elite ? 45 : 30);
    reward.goldAvailable = true;
    reward.goldAmount = goldDist(rng);

    if (HasPotionSlot(run)) {
        std::uniform_int_distribution<int> potionRoll(1, 100);
        const int potionChance = roomType == RunNodeType::Elite ? 50 : 30;
        if (potionRoll(rng) <= potionChance) {
            reward.potionAvailable = true;
            reward.potion = potionPool[static_cast<size_t>(rng() % potionPool.size())];
        }
    }

    reward.cardRewardAvailable = (roomType != RunNodeType::Boss);
    if (reward.cardRewardAvailable) {
        const vector<CardData> favoredPool = CardLibrary::BuildArchetypeRewardPool(run.selectedCardPackArchetype);
        if (!favoredPool.empty()) {
            const CardData favoredCard = PickWeightedUniqueRewardCard(rng, favoredPool, reward.cardChoices);
            if (favoredCard.id != 0 || !favoredCard.name.empty()) {
                reward.cardChoices.push_back(favoredCard);
            }
        }

        while (reward.cardChoices.size() < 3 && reward.cardChoices.size() < cardPool.size()) {
            const CardData pickedCard = PickWeightedUniqueRewardCard(rng, cardPool, reward.cardChoices);
            if (pickedCard.id == 0 && pickedCard.name.empty()) {
                break;
            }
            reward.cardChoices.push_back(pickedCard);
        }
    }

    return reward;
}

bool HasBattleRewardItemsRemaining(const BattleRewardState& reward) {
    bool unclaimedRelic = false;
    for (size_t index = 0; index < reward.relicRewards.size(); ++index) {
        const bool claimed = (index < reward.relicClaimed.size()) && (reward.relicClaimed[index] != 0);
        if (!claimed) {
            unclaimedRelic = true;
            break;
        }
    }

    return (reward.goldAvailable && !reward.goldClaimed) ||
        (reward.potionAvailable && !reward.potionClaimed) ||
        (reward.cardRewardAvailable && !reward.cardRewardClaimed) ||
        unclaimedRelic;
}

string BuildPotionActionText(const PotionData& potion) {
    if (potion.name == u8"회복 포션") {
        return u8"회복 +12";
    }
    if (potion.name == u8"에너지 포션") {
        return u8"에너지 +2";
    }
    if (potion.name == u8"연막 포션") {
        return u8"도주";
    }
    return potion.description;
}

void RenderAsciiArtLines(ScreenManager& screen, const Rect& rect, const vector<string>& lines, WORD color) {
    if (lines.empty()) {
        RenderWrappedText(screen, rect.x + 2, rect.y + 2, rect.width - 4, u8"<그림>", color);
        return;
    }

    int drawY = rect.y + 2;
    for (const string& line : lines) {
        if (drawY >= rect.y + rect.height - 1) {
            break;
        }
        screen.DrawString(rect.x + 2, drawY, line, color);
        ++drawY;
    }
}

void RenderTextBlock(ScreenManager& screen, const Rect& rect, const vector<string>& lines, WORD color) {
    int drawY = rect.y;
    for (const string& line : lines) {
        const TextLayout::WrappedText wrapped = TextLayout::WrapUtf8(line, rect.width);
        for (const std::wstring& wrappedLine : wrapped.lines) {
            if (drawY >= rect.y + rect.height) {
                return;
            }
            screen.DrawString(rect.x, drawY, wrappedLine, color);
            ++drawY;
        }
    }
}

bool RenderWrappedActionButton(
    ScreenManager& screen,
    InputManager& input,
    const Rect& rect,
    int mouseX,
    int mouseY,
    const std::vector<std::string>& lines,
    WORD idleColor,
    WORD hoverColor,
    bool interactive,
    TextLayout::HorizontalAlign align = TextLayout::HorizontalAlign::Center) {
    const bool hovered = rect.Contains(mouseX, mouseY);
    const WORD color = hovered ? hoverColor : idleColor;
    RenderFrameBox(screen, rect, color);

    std::vector<std::wstring> wrappedLines;
    for (const std::string& line : lines) {
        const TextLayout::WrappedText wrapped = TextLayout::WrapUtf8(line, rect.width - 4);
        wrappedLines.insert(wrappedLines.end(), wrapped.lines.begin(), wrapped.lines.end());
    }

    const int contentHeight = static_cast<int>(wrappedLines.size());
    int drawY = TextLayout::ComputeAlignedY(rect.y + 1, rect.height - 2, contentHeight);
    for (const std::wstring& wrappedLine : wrappedLines) {
        screen.DrawString(
            rect.x + 2,
            drawY,
            TextLayout::AlignToWidth(wrappedLine, rect.width - 4, align),
            color);
        ++drawY;
        if (drawY >= rect.y + rect.height - 1) {
            break;
        }
    }

    return interactive && hovered && input.IsLeftClickDown();
}

std::vector<std::string> BuildRunRecordDetailLines(const RunRecordData& record) {
    return {
        string(u8"날짜: ") + record.timestampText,
        string(u8"시드: ") + to_string(record.seed),
        string(u8"플레이 시간: ") + FormatPlayTime(record.playTimeSec),
        string(u8"도달 층: ") + to_string(record.reachedFloor),
        record.failureReasonText.empty() ? u8"패배 원인: 없음" : (string(u8"패배 원인: ") + record.failureReasonText),
        string(u8"유물: ") + BuildRelicListText(record.relics),
        string(u8"방문 노드: ") + BuildNodeVisitedText(record.visitedNodes),
        string(u8"덱 장수: ") + to_string(static_cast<int>(record.deckSnapshot.size()))
    };
}

ScreenMode ParseScreenMode(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        const string arg = argv[i] != nullptr ? argv[i] : "";
        if (arg == "--fullscreen" || arg == "/fullscreen") {
            return ScreenMode::Fullscreen;
        }
        if (arg == "--windowed" || arg == "/windowed") {
            return ScreenMode::WindowedNearFullscreen;
        }
    }

    return ScreenMode::Fullscreen;
}

} // namespace

// 실제 게임 루프 진입점.
int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);

    ScreenManager screen(ParseScreenMode(argc, argv));
    InputManager input;
    AudioManager audio;
    TooltipUI tooltip;
    MapRenderer mapRenderer(screen.GetWidth(), screen.GetHeight());

    SettingsData settings = SaveManager::LoadSettings();
    GlobalStatsData globalStats = SaveManager::LoadGlobalStats();
    vector<RunRecordData> runRecords = SaveManager::LoadRunRecords();
    vector<CardPackOption> starterPacks = BuildStarterCardPacks();

    audio.SetVolumes(settings.masterVolume, settings.bgmVolume, settings.sfxVolume);
    audio.PlayBGM(L"Blitz of Card.wav", 100.0f);

    AppState appState = AppState::Title;
    TitleOverlayType titleOverlay = TitleOverlayType::None;
    RunStateData run = {};
    bool hasContinueRun = SaveManager::HasContinueRun();
    bool shouldQuit = false;
    int selectedRecordIndex = 0;
    int recordsScroll = 0;
    int deckScroll = 0;
    int activeSliderId = -1;
    float runPlayAccumulatorSec = 0.0f;
    bool settingsDirty = false;
    std::unique_ptr<CombatSystem> combatSystem;
    std::unique_ptr<EntityUI> playerEntityUi;
    std::unique_ptr<EntityUI> enemyEntityUi;
    TargetingArrow targetingArrow;
    int activeCombatRoomKey = -9999;
    int draggedHandIndex = -1;
    std::vector<int> handLayoutXs;
    bool nextDrawCardGoesRight = true;
    bool forceRecenterHandLayout = true;
    float discardPileExpandProgress = 0.0f;
    float drawPileExpandProgress = 0.0f;
    std::wstring lastQueuedBgmTrack;
    int lastQueuedBgmPercent = -1;
    bool wasF1Pressed = false;
    bool wasF2Pressed = false;
    bool wasF3Pressed = false;
    bool wasF4Pressed = false;
    bool wasF5Pressed = false;
    bool wasF6Pressed = false;

    bool wasF7Pressed = false;
    bool wasF8Pressed = false;
    bool wasF9Pressed = false;
    bool wasF10Pressed = false;
    bool wasF11Pressed = false;
    bool wasF12Pressed = false;
    EndingStage endingStage = EndingStage::None;
    RunRecordData endingRecord = {};
    std::string endingFlavorText;
    float endingRevealProgress = 0.0f;
    int animatedPackIndex = -1;
    float cardPackPanelProgress = 0.0f;
    std::string powerBannerText;
    float powerBannerTimerSec = 0.0f;
    bool artPreviewOpen = false;
    int artPreviewIndex = 0;
    std::mt19937 audioRng(std::random_device{}());
    int audioAliasCounter = 0;
    std::unordered_map<std::string, bool> hoverAudioLatch;
    bool titleWindActive = false;
    bool restFireActive = false;
    std::wstring restFireLoopFile;
    int lastHoveredMapNodeId = -1;
    bool lastMapOverlayOpen = false;
    bool lastDeckOverlayOpen = false;
    bool lastShopUiOpen = false;
    bool lastShopRemoveMode = false;
    bool neowVoicePlayed = false;
    const std::vector<std::wstring> uiHoverPool = { L"sfx\\SOTE_SFX_UIHover_v2.wav" };
    const std::vector<std::wstring> uiClickPool = { L"sfx\\SOTE_SFX_UIClick_1_v2.wav", L"sfx\\SOTE_SFX_UIClick_2_v2.wav" };
    const std::vector<std::wstring> cardSelectPool = { L"sfx\\SOTE_SFX_CardSelect_v2.ogg" };
    const std::vector<std::wstring> cardRejectPool = { L"sfx\\SOTE_SFX_CardReject_v1.ogg" };
    const std::vector<std::wstring> gainDefensePool = {
        L"sfx\\SOTE_SFX_GainDefense_RR1_v3.ogg",
        L"sfx\\SOTE_SFX_GainDefense_RR2_v3.ogg",
        L"sfx\\SOTE_SFX_GainDefense_RR3_v3.ogg"
    };
    const std::vector<std::wstring> buffPool = {
        L"sfx\\SOTE_SFX_Buff_1_v1.ogg",
        L"sfx\\SOTE_SFX_Buff_2_v1.ogg",
        L"sfx\\SOTE_SFX_Buff_3_v1.ogg"
    };
    const std::vector<std::wstring> debuffPool = {
        L"sfx\\SOTE_SFX_Debuff_1_v1.ogg",
        L"sfx\\SOTE_SFX_Debuff_2_v1.ogg",
        L"sfx\\SOTE_SFX_Debuff_3_v1.ogg"
    };
    const std::vector<std::wstring> potionDropPool = {
        L"sfx\\SOTE_SFX_DropPotion_1_v1.ogg",
        L"sfx\\SOTE_SFX_DropPotion_2_v1.ogg"
    };
    const std::vector<std::wstring> relicDropPool = {
        L"sfx\\SOTE_SFX_DropRelic_Clink.ogg",
        L"sfx\\SOTE_SFX_DropRelic_Flat.ogg",
        L"sfx\\SOTE_SFX_DropRelic_Heavy.ogg",
        L"sfx\\SOTE_SFX_DropRelic_Magical.ogg",
        L"sfx\\SOTE_SFX_DropRelic_Rocky.ogg"
    };
    const std::vector<std::wstring> healShortPool = {
        L"sfx\\SOTE_SFX_HealShort_1_v2.ogg",
        L"sfx\\SOTE_SFX_HealShort_2_v2.ogg",
        L"sfx\\SOTE_SFX_HealShort_3_v2.ogg"
    };
    const std::vector<std::wstring> goldRewardPool = {
        L"sfx\\SOTE_SFX_Gold_RR1_v3.ogg",
        L"sfx\\SOTE_SFX_Gold_RR2_v3.ogg",
        L"sfx\\SOTE_SFX_Gold_RR3_v3.ogg",
        L"sfx\\SOTE_SFX_Gold_RR4_v3.ogg",
        L"sfx\\SOTE_SFX_Gold_RR5_v3.ogg"
    };
    const std::vector<std::wstring> mapHoverPool = {
        L"sfx\\SOTE_SFX_MapHover_1_v1.ogg",
        L"sfx\\SOTE_SFX_MapHover_2_v1.ogg",
        L"sfx\\SOTE_SFX_MapHover_3_v1.ogg",
        L"sfx\\SOTE_SFX_MapHover_4_v1.ogg"
    };
    const std::vector<std::wstring> mapSelectPool = {
        L"sfx\\SOTE_SFX_MapSelect_1_v1.ogg",
        L"sfx\\SOTE_SFX_MapSelect_2_v1.ogg",
        L"sfx\\SOTE_SFX_MapSelect_3_v1.ogg",
        L"sfx\\SOTE_SFX_MapSelect_4_v1.ogg"
    };
    const std::vector<std::wstring> mapOpenPool = {
        L"sfx\\SOTE_SFX_Map_1_v3.ogg",
        L"sfx\\SOTE_SFX_Map_2_v3.ogg"
    };
    const std::vector<std::wstring> battleStartPool = {
        L"sfx\\STS_SFX_BattleStart_1_v1.ogg",
        L"sfx\\STS_SFX_BattleStart_2_v1.ogg"
    };
    const std::vector<std::wstring> potionUsePool = {
        L"sfx\\SOTE_SFX_Potion_1_v2.ogg",
        L"sfx\\SOTE_SFX_Potion_2_v2.ogg",
        L"sfx\\SOTE_SFX_Potion_3_v2.ogg"
    };
    const std::vector<std::wstring> restFirePool = {
        L"sfx\\SOTE_SFX_RestFireDry_v2.ogg",
        L"sfx\\SOTE_SFX_RestFireWet_v2.ogg"
    };
    const std::vector<std::wstring> parchmentPool = {
        L"sfx\\SOTE_SFX_UI_Parchment_1_v2.ogg",
        L"sfx\\SOTE_SFX_UI_Parchment_2_v1.ogg",
        L"sfx\\SOTE_SFX_UI_Parchment_3_v1.ogg"
    };
    const std::vector<std::wstring> sleepJinglePool = {
        L"sfx\\STS_SleepJingle_1a_NewMix_v1.ogg",
        L"sfx\\STS_SleepJingle_1b_NewMix_v1.ogg",
        L"sfx\\STS_SleepJingle_1c_NewMix_v1.ogg"
    };
    const std::vector<std::wstring> merchantIdleVoicePool = {
        L"voice\\STS_VO_Merchant_Mlyah_a.ogg",
        L"voice\\STS_VO_Merchant_Mlyah_b.ogg",
        L"voice\\STS_VO_Merchant_Mlyah_c.ogg"
    };
    const std::vector<std::wstring> merchantOpenVoicePool = {
        L"voice\\STS_VO_Merchant_3a.ogg",
        L"voice\\STS_VO_Merchant_3b.ogg",
        L"voice\\STS_VO_Merchant_3c.ogg"
    };
    const std::vector<std::wstring> merchantBuyVoicePool = {
        L"voice\\STS_VO_Merchant_Kekeke_a.ogg",
        L"voice\\STS_VO_Merchant_Kekeke_b.ogg",
        L"voice\\STS_VO_Merchant_Kekeke_c.ogg"
    };
    const std::vector<std::wstring> merchantNoGoldVoicePool = {
        L"voice\\STS_VO_Merchant_2a.ogg",
        L"voice\\STS_VO_Merchant_2b.ogg",
        L"voice\\STS_VO_Merchant_2c.ogg"
    };
    const std::vector<std::wstring> neowVoicePool = {
        L"voice\\STS_VO_Neow_1a.ogg",
        L"voice\\STS_VO_Neow_1b.ogg",
        L"voice\\STS_VO_Neow_2a.ogg",
        L"voice\\STS_VO_Neow_2b.ogg",
        L"voice\\STS_VO_Neow_3a.ogg",
        L"voice\\STS_VO_Neow_3b.ogg"
    };
    const std::vector<std::wstring> deathMusicPool = {
        L"BGM\\STS_DeathStinger_1_v3_MUSIC.ogg",
        L"BGM\\STS_DeathStinger_2_v3_MUSIC.ogg",
        L"BGM\\STS_DeathStinger_3_v3_MUSIC.ogg",
        L"BGM\\STS_DeathStinger_4_v3_MUSIC.ogg"
    };

    auto reloadRecords = [&]() {
        runRecords = SaveManager::LoadRunRecords();
        selectedRecordIndex = (std::min)(selectedRecordIndex, (std::max)(0, static_cast<int>(runRecords.size()) - 1));
        if (selectedRecordIndex < 0) {
            selectedRecordIndex = 0;
        }
        };

    auto resetCombatPresentation = [&]() {
        combatSystem.reset();
        playerEntityUi.reset();
        enemyEntityUi.reset();
        activeCombatRoomKey = -9999;
        draggedHandIndex = -1;
        handLayoutXs.clear();
        nextDrawCardGoesRight = true;
        forceRecenterHandLayout = true;
        discardPileExpandProgress = 0.0f;
        drawPileExpandProgress = 0.0f;
        powerBannerText.clear();
        powerBannerTimerSec = 0.0f;
        targetingArrow.SetActive(false);
        };

    auto queueRoomBgm = [&]() {
        std::wstring track = L"Blitz of Card.wav";
        int targetPercent = 100;
        float fadeOutSec = 0.2f;
        float fadeInSec = 0.35f;

        if (appState != AppState::Run) {
            if (lastQueuedBgmTrack != track || lastQueuedBgmPercent != targetPercent) {
                audio.QueueBGMFade(track, static_cast<float>(targetPercent), fadeOutSec, 0.45f);
                lastQueuedBgmTrack = track;
                lastQueuedBgmPercent = targetPercent;
            }
            return;
        }

        if (run.scene == RunSceneType::CardPackSelect || run.currentNodeId < 0 || run.overlay == RunOverlayType::Map || run.overlay == RunOverlayType::Deck || run.overlay == RunOverlayType::Settings || run.overlay == RunOverlayType::Confirm) {
            track = L"Exordium.wav";
            targetPercent = 50;
        }
        else if (run.overlay == RunOverlayType::Ending) {
            track = (run.currentRoomType == RunNodeType::Boss && run.won) ? L"The Guardian Emerges.wav" : L"Exordium.wav";
            targetPercent = 40;
        }
        else if (!run.roomResolved && run.currentRoomType == RunNodeType::Shop) {
            track = L"BGM\\STS_Merchant_NewMix_v1.ogg";
            targetPercent = 100;
            fadeOutSec = 0.15f;
            fadeInSec = 0.25f;
        }
        else if (!run.roomResolved && run.currentRoomType == RunNodeType::Event) {
            track = L"BGM\\SOTE_Level1_Ambience_v6.ogg";
            targetPercent = 70;
            fadeOutSec = 0.2f;
            fadeInSec = 0.3f;
        }
        else if (!run.roomResolved && run.currentRoomType == RunNodeType::Boss) {
            track = L"The Guardian Emerges.wav";
            targetPercent = 100;
            fadeOutSec = 0.25f;
            fadeInSec = 0.4f;
        }
        else if (!run.roomResolved && IsBattleNodeType(run.currentRoomType)) {
            track = L"Exordium.wav";
            targetPercent = 100;
            fadeInSec = 0.3f;
        }
        else {
            track = L"Exordium.wav";
            targetPercent = 50;
        }

        if (lastQueuedBgmTrack != track || lastQueuedBgmPercent != targetPercent) {
            audio.QueueBGMFade(track, static_cast<float>(targetPercent), fadeOutSec, fadeInSec);
            lastQueuedBgmTrack = track;
            lastQueuedBgmPercent = targetPercent;
        }
        };

    auto startNewRun = [&]() {
        CreateNewRun(run, BuildSeed(settings), screen.GetWidth(), screen.GetHeight());
        SaveManager::SaveContinueRun(run);
        hasContinueRun = true;
        mapRenderer.SetNodes(&run.nodes);
        mapRenderer.FocusToFloor(1);
        resetCombatPresentation();
        activeSliderId = -1;
        tooltip.SetVisible(false);
        appState = AppState::Run;
        titleOverlay = TitleOverlayType::None;
        deckScroll = 0;
        runPlayAccumulatorSec = 0.0f;
        endingStage = EndingStage::None;
        endingRecord = {};
        endingFlavorText.clear();
        endingRevealProgress = 0.0f;
        animatedPackIndex = -1;
        cardPackPanelProgress = 0.0f;
        neowVoicePlayed = false;
        lastHoveredMapNodeId = -1;
        hoverAudioLatch.clear();
        queueRoomBgm();
        };

    auto loadContinueRun = [&]() {
        RunStateData loadedRun = {};
        if (!SaveManager::LoadContinueRun(loadedRun)) {
            hasContinueRun = false;
            return false;
        }

        run = loadedRun;
        mapRenderer.SetNodes(&run.nodes);
        if (run.currentFloor > 0) {
            mapRenderer.FocusToFloor(run.currentFloor);
        }
        resetCombatPresentation();
        tooltip.SetVisible(false);
        appState = AppState::Run;
        titleOverlay = TitleOverlayType::None;
        deckScroll = 0;
        runPlayAccumulatorSec = 0.0f;
        endingStage = EndingStage::None;
        endingRecord = {};
        endingFlavorText.clear();
        endingRevealProgress = 0.0f;
        animatedPackIndex = (run.selectedStarterPackIndex >= 0) ? run.selectedStarterPackIndex : -1;
        cardPackPanelProgress = (run.selectedStarterPackIndex >= 0) ? 1.0f : 0.0f;
        neowVoicePlayed = (run.scene != RunSceneType::CardPackSelect);
        lastHoveredMapNodeId = -1;
        hoverAudioLatch.clear();
        queueRoomBgm();
        return true;
        };

    auto transitionToTitle = [&]() {
        appState = AppState::Title;
        run = {};
        titleOverlay = TitleOverlayType::None;
        mapRenderer.SetNodes(nullptr);
        resetCombatPresentation();
        tooltip.SetVisible(false);
        hasContinueRun = SaveManager::HasContinueRun();
        endingStage = EndingStage::None;
        endingRecord = {};
        endingFlavorText.clear();
        endingRevealProgress = 0.0f;
        animatedPackIndex = -1;
        cardPackPanelProgress = 0.0f;
        neowVoicePlayed = false;
        lastHoveredMapNodeId = -1;
        hoverAudioLatch.clear();
        audio.QueueBGMFade(L"Blitz of Card.wav", 100.0f, 0.2f, 0.45f);
        lastQueuedBgmTrack = L"Blitz of Card.wav";
        lastQueuedBgmPercent = 100;
        };

    auto finishRunToEnding = [&](bool won, const string& failureReasonText) {
        run.finished = true;
        run.won = won;

        RunRecordData record = BuildRunRecord(run, won, failureReasonText);
        endingRecord = record;

        if (!run.loseRecordCommitted || won) {
            SaveManager::AppendRunRecord(record, globalStats);
            reloadRecords();
            run.loseRecordCommitted = true;
        }

        SaveManager::DeleteContinueRun();
        hasContinueRun = false;
        tooltip.SetVisible(false);
        activeSliderId = -1;
        run.pendingConfirm = ConfirmActionType::None;
        run.overlay = RunOverlayType::Ending;
        appState = AppState::Run;
        endingRevealProgress = 0.0f;
        endingFlavorText = ((run.seed + static_cast<std::uint32_t>(run.currentFloor) + (won ? 1u : 0u)) % 2 == 0)
            ? u8"당신은 죽었습니다!"
            : u8"더럽혀짐";
        endingStage = won ? EndingStage::VictoryReveal : EndingStage::DefeatReveal;
        audio.FadeCurrentBGMTo(won ? 0.0f : 20.0f, won ? 0.5f : 0.35f);
        };

    auto abandonContinueRunFromTitle = [&]() {
        SaveManager::DeleteContinueRun();
        hasContinueRun = false;
        titleOverlay = TitleOverlayType::None;
        };

    auto abandonRunFromSettings = [&]() {
        const string failureReason = to_string((std::max)(1, run.currentFloor)) + u8"층에서 도전을 포기하셨습니다.";
        finishRunToEnding(false, failureReason);
        };

    auto saveAndExitRun = [&]() {
        SaveManager::SaveContinueRun(run);
        hasContinueRun = true;
        transitionToTitle();
        };

    using Clock = chrono::steady_clock;
    const int targetRefreshRate = GetTargetRefreshRate();
    const auto targetFrameTime = chrono::microseconds(1000000 / targetRefreshRate);
    auto previousFrameTime = Clock::now();
    auto fpsSampleStartTime = previousFrameTime;
    int sampledFrames = 0;
    double displayedFps = 0.0;
    double displayedFrameMs = 0.0;

    while (!shouldQuit) {
        const auto frameStartTime = Clock::now();
        displayedFrameMs = chrono::duration<double, milli>(frameStartTime - previousFrameTime).count();
        previousFrameTime = frameStartTime;
        ++sampledFrames;

        const double fpsSampleSeconds = chrono::duration<double>(frameStartTime - fpsSampleStartTime).count();
        if (fpsSampleSeconds >= 0.25) {
            displayedFps = sampledFrames / fpsSampleSeconds;
            sampledFrames = 0;
            fpsSampleStartTime = frameStartTime;
        }

        const float deltaTimeSec = static_cast<float>(displayedFrameMs * 0.001);
        audio.Update(deltaTimeSec);

        input.Update();
        const int mouseX = input.GetMouseX();
        const int mouseY = input.GetMouseY();

        audio.SetVolumes(settings.masterVolume, settings.bgmVolume, settings.sfxVolume);

        if (appState == AppState::Run) {
            runPlayAccumulatorSec += deltaTimeSec;
            while (runPlayAccumulatorSec >= 1.0f) {
                ++run.playTimeSec;
                runPlayAccumulatorSec -= 1.0f;
            }
        }

        const bool pressedEsc = input.IsEscPressedDown();
        const bool pressedMapHotkey = input.IsMapHotkeyPressedDown();
        const bool pressedF1 = ConsumeKeyPress(VK_F1, wasF1Pressed);
        const bool pressedF2 = ConsumeKeyPress(VK_F2, wasF2Pressed);
        const bool pressedF3 = ConsumeKeyPress(VK_F3, wasF3Pressed);
        const bool pressedF4 = ConsumeKeyPress(VK_F4, wasF4Pressed);
        const bool pressedF5 = ConsumeKeyPress(VK_F5, wasF5Pressed);
        const bool pressedF6 = ConsumeKeyPress(VK_F6, wasF6Pressed);
        const bool pressedF7 = ConsumeKeyPress(VK_F7, wasF7Pressed);
        const bool pressedF8 = ConsumeKeyPress(VK_F8, wasF8Pressed);
        const bool pressedF9 = ConsumeKeyPress(VK_F9, wasF9Pressed);
        const bool pressedF10 = ConsumeKeyPress(VK_F10, wasF10Pressed);
        const bool pressedF11 = ConsumeKeyPress(VK_F11, wasF11Pressed);
        const bool pressedF12 = ConsumeKeyPress(VK_F12, wasF12Pressed);
        const bool ctrlHeld = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool pressedLeft = (GetAsyncKeyState(VK_LEFT) & 0x0001) != 0;
        const bool pressedRight = (GetAsyncKeyState(VK_RIGHT) & 0x0001) != 0;

        if (ctrlHeld && pressedF11) {
            artPreviewOpen = !artPreviewOpen;
        }
        if (artPreviewOpen) {
            const int previewCount = static_cast<int>(GetArtPreviewEntries().size());
            if (previewCount > 0) {
                if (ctrlHeld && pressedF12) {
                    artPreviewIndex = (artPreviewIndex + 1) % previewCount;
                }
                if (pressedRight) {
                    artPreviewIndex = (artPreviewIndex + 1) % previewCount;
                }
                if (pressedLeft) {
                    artPreviewIndex = (artPreviewIndex - 1 + previewCount) % previewCount;
                }
            }
        }

        if (appState == AppState::Run && settings.debugMode && run.overlay == RunOverlayType::None) {
            const auto openDebugRoom = [&](RunNodeType type) {
                run.currentRoomType = type;
                run.currentRoomResult = RunNodeResultType::None;
                run.roomResolved = false;
                run.currentNodeId = -1;
                run.scene = RunSceneType::Room;
                ResetRoomRuntimeState(run);
                PrepareCurrentRoomState(run);
                resetCombatPresentation();
                queueRoomBgm();
                };

            const auto openCardPackPreview = [&]() {
                run.scene = RunSceneType::CardPackSelect;
                run.currentNodeId = -1;
                run.currentRoomResult = RunNodeResultType::None;
                run.roomResolved = false;
                run.overlay = RunOverlayType::None;
                ResetRoomRuntimeState(run);
                resetCombatPresentation();
                tooltip.SetVisible(false);
                animatedPackIndex = (run.selectedStarterPackIndex >= 0) ? run.selectedStarterPackIndex : -1;
                cardPackPanelProgress = (run.selectedStarterPackIndex >= 0) ? 1.0f : 0.0f;
                queueRoomBgm();
                };

            if (pressedF1) {
                run.player.currentHp = (std::min)(run.player.maxHp, run.player.currentHp + 10);
            }
            if (pressedF2) {
                run.player.currentHp = (std::max)(1, run.player.currentHp - 10);
            }
            if (pressedF3) {
                run.gold += 100;
            }
            if (pressedF4 && HasPotionSlot(run)) {
                const vector<PotionData> debugPotions = BuildBattleRewardPotionPool();
                run.potions.push_back(debugPotions[static_cast<size_t>(run.potions.size() % debugPotions.size())]);
            }
            if (!ctrlHeld && pressedF5 && combatSystem && !run.roomResolved && IsBattleNodeType(run.currentRoomType)) {
                run.battleRoom.enemy.currentHp = 0;
            }
            if (!ctrlHeld && pressedF6 && combatSystem && !run.roomResolved && IsBattleNodeType(run.currentRoomType)) {
                run.player.currentHp = 0;
            }
            if (!ctrlHeld && pressedF7) openDebugRoom(RunNodeType::Shop);
            if (!ctrlHeld && pressedF8) openDebugRoom(RunNodeType::Rest);
            if (!ctrlHeld && pressedF9) openDebugRoom(RunNodeType::Treasure);
            if (!ctrlHeld && pressedF10) openDebugRoom(RunNodeType::Battle);
            if (!ctrlHeld && pressedF11) openDebugRoom(RunNodeType::Elite);
            if (!ctrlHeld && pressedF12) openDebugRoom(RunNodeType::Boss);

            if (ctrlHeld && pressedF7) openDebugRoom(RunNodeType::Event);
            if (ctrlHeld && pressedF8) openCardPackPreview();
            if (ctrlHeld && pressedF9) {
                finishRunToEnding(true, "");
            }
            if (ctrlHeld && pressedF10) {
                finishRunToEnding(false, to_string((std::max)(1, run.currentFloor)) + u8"층 디버그 패배 미리보기");
            }
        }

        if (appState == AppState::Run && run.overlay != RunOverlayType::Ending) {
            queueRoomBgm();
        }

        if (appState == AppState::Title && settings.effectsEnabled) {
            if (!titleWindActive) {
                PlayFixedEffect(audio, L"sfx\\SOTE_SFX_WindAmb_v1.ogg", L"amb_title_wind", audioAliasCounter, true, true);
                titleWindActive = true;
            }
        }
        else if (titleWindActive) {
            StopLoopEffect(audio, L"amb_title_wind");
            titleWindActive = false;
        }

        const bool mapOverlayOpen = (appState == AppState::Run && run.overlay == RunOverlayType::Map);
        if (mapOverlayOpen && !lastMapOverlayOpen) {
            PlayRandomEffect(audio, mapOpenPool, L"sfx_map_open", audioRng, audioAliasCounter, settings.effectsEnabled);
        }
        lastMapOverlayOpen = mapOverlayOpen;

        const bool deckOverlayOpen = (appState == AppState::Run && run.overlay == RunOverlayType::Deck);
        if (deckOverlayOpen && !lastDeckOverlayOpen) {
            PlayRandomEffect(audio, parchmentPool, L"sfx_deck_open", audioRng, audioAliasCounter, settings.effectsEnabled);
        }
        lastDeckOverlayOpen = deckOverlayOpen;

        const bool shopUiOpenNow =
            (appState == AppState::Run &&
                run.scene == RunSceneType::Room &&
                !run.roomResolved &&
                run.currentRoomType == RunNodeType::Shop &&
                run.shopRoom.uiOpen);
        if (shopUiOpenNow != lastShopUiOpen) {
            PlayFixedEffect(
                audio,
                shopUiOpenNow ? L"sfx\\SOTE_SFX_ShopRugOpen_v1.ogg" : L"sfx\\SOTE_SFX_ShopRugClose_v1.ogg",
                shopUiOpenNow ? L"sfx_shop_open" : L"sfx_shop_close",
                audioAliasCounter,
                settings.effectsEnabled);
        }
        lastShopUiOpen = shopUiOpenNow;

        const bool shopRemoveModeNow =
            (appState == AppState::Run &&
                run.scene == RunSceneType::Room &&
                !run.roomResolved &&
                run.currentRoomType == RunNodeType::Shop &&
                run.shopRoom.uiOpen &&
                run.shopRoom.removeMode);
        if (shopRemoveModeNow && !lastShopRemoveMode) {
            PlayRandomEffect(audio, parchmentPool, L"sfx_shop_remove_open", audioRng, audioAliasCounter, settings.effectsEnabled);
        }
        lastShopRemoveMode = shopRemoveModeNow;

        const bool restFireShouldPlay =
            (settings.effectsEnabled &&
                appState == AppState::Run &&
                run.scene == RunSceneType::Room &&
                !run.roomResolved &&
                run.currentRoomType == RunNodeType::Rest);
        if (restFireShouldPlay) {
            if (restFireLoopFile.empty()) {
                restFireLoopFile = PickRandomAudio(restFirePool, audioRng);
            }
            if (!restFireActive) {
                PlayFixedEffect(audio, restFireLoopFile, L"amb_rest_fire", audioAliasCounter, true, true);
                restFireActive = true;
            }
            audio.UpdateSpatialVolume(screen.GetCenterX() + 18, screen.GetHeight() - 6, mouseX, mouseY, L"amb_rest_fire", 36);
        }
        else if (restFireActive) {
            StopLoopEffect(audio, L"amb_rest_fire");
            restFireActive = false;
            restFireLoopFile.clear();
        }

        screen.Clear();

        if (appState == AppState::Title) {
            if (pressedEsc && titleOverlay == TitleOverlayType::None) {
                shouldQuit = true;
            }

            const std::vector<std::string>& titleLogo = AsciiArtLibrary::Get(AsciiArtId::TitleLogo);
            const Rect titleLogoClip = { 0, 0, screen.GetWidth(), screen.GetHeight() };
            RenderCenteredArtClipped(screen, screen.GetCenterX(), screen.GetCenterY() - 1, titleLogo, COLOR_WHITE, titleLogoClip);

            vector<ButtonUI> titleButtons;
            int buttonX = 4;
            int buttonY = screen.GetHeight() - 20;
            const int buttonWidth = 18;
            const int buttonHeight = 3;
            auto addTitleButton = [&](const string& text) {
                titleButtons.emplace_back(buttonX, buttonY, buttonWidth, buttonHeight, text, COLOR_WHITE, COLOR_YELLOW);
                buttonY += 4;
                };

            if (hasContinueRun) {
                addTitleButton(u8"계속");
                addTitleButton(u8"전투를 포기");
            }
            else {
                addTitleButton(u8"시작");
            }
            addTitleButton(u8"설정");
            addTitleButton(u8"기록");
            addTitleButton(u8"종료");

            const bool titleButtonsInteractive = (titleOverlay == TitleOverlayType::None);
            for (size_t buttonIndex = 0; buttonIndex < titleButtons.size(); ++buttonIndex) {
                ButtonUI& button = titleButtons[buttonIndex];
                if (titleButtonsInteractive) {
                    button.Update(input);
                    UpdateButtonAudio(button, "title_main_" + to_string(buttonIndex), audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                }
                button.Render(screen);
            }

            if (titleButtonsInteractive) {
                int titleButtonCursor = 0;
                if (hasContinueRun) {
                    if (titleButtons[titleButtonCursor++].IsClicked()) {
                        loadContinueRun();
                    }
                    if (titleButtons[titleButtonCursor++].IsClicked()) {
                        titleOverlay = TitleOverlayType::ConfirmAbandon;
                    }
                }
                else if (titleButtons[titleButtonCursor++].IsClicked()) {
                    if (settings.enableCharacterSelect) {
                        titleOverlay = TitleOverlayType::CharacterSelect;
                    }
                    else if (settings.enableAscensionSelect) {
                        titleOverlay = TitleOverlayType::AscensionSelect;
                    }
                    else {
                        startNewRun();
                    }
                }

                if (titleButtons[titleButtonCursor++].IsClicked()) {
                    titleOverlay = TitleOverlayType::Settings;
                }
                if (titleButtons[titleButtonCursor++].IsClicked()) {
                    titleOverlay = TitleOverlayType::Records;
                }
                if (titleButtons[titleButtonCursor++].IsClicked()) {
                    shouldQuit = true;
                }
            }

            if (titleOverlay == TitleOverlayType::Settings) {
                const Rect popup = { screen.GetCenterX() - 34, 7, 68, 24 };
                RenderFrameBox(screen, popup, COLOR_YELLOW);
                RenderPanelTitle(screen, popup, u8"설정", COLOR_YELLOW);

                const int contentLeft = popup.x + 4;
                const int contentWidth = popup.width - 8;
                Rect masterTrack = { contentLeft + 12, popup.y + 5, kTitleSettingsSliderWidth, 1 };
                Rect bgmTrack = { contentLeft + 12, popup.y + 7, kTitleSettingsSliderWidth, 1 };
                Rect sfxTrack = { contentLeft + 12, popup.y + 9, kTitleSettingsSliderWidth, 1 };
                Rect speedTrack = { contentLeft + 12, popup.y + 11, kTitleSettingsSliderWidth, 1 };

                UpdateSliderDrag(input, mouseX, 0, masterTrack, settings.masterVolume, activeSliderId);
                UpdateSliderDrag(input, mouseX, 1, bgmTrack, settings.bgmVolume, activeSliderId);
                UpdateSliderDrag(input, mouseX, 2, sfxTrack, settings.sfxVolume, activeSliderId);
                UpdateSliderDrag(input, mouseX, 3, speedTrack, settings.gameSpeedPercent, activeSliderId);

                RenderSlider(screen, contentLeft, popup.y + 5, kTitleSettingsSliderWidth, u8"마스터", settings.masterVolume, activeSliderId == 0);
                RenderSlider(screen, contentLeft, popup.y + 7, kTitleSettingsSliderWidth, u8"BGM", settings.bgmVolume, activeSliderId == 1);
                RenderSlider(screen, contentLeft, popup.y + 9, kTitleSettingsSliderWidth, u8"SFX", settings.sfxVolume, activeSliderId == 2);
                RenderSlider(screen, contentLeft, popup.y + 11, kTitleSettingsSliderWidth, u8"속도", settings.gameSpeedPercent, activeSliderId == 3);

                ButtonUI btnToggleEffects(contentLeft, popup.y + 14, 18, 3, string(u8"이펙트 ") + (settings.effectsEnabled ? u8"ON" : u8"OFF"), COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnToggleDebug(contentLeft + 20, popup.y + 14, 18, 3, string(u8"디버그 ") + (settings.debugMode ? u8"ON" : u8"OFF"), COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnToggleChar(contentLeft, popup.y + 18, 18, 3, string(u8"캐릭 선택 ") + (settings.enableCharacterSelect ? u8"ON" : u8"OFF"), COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnToggleAsc(contentLeft + 20, popup.y + 18, 18, 3, string(u8"승천 선택 ") + (settings.enableAscensionSelect ? u8"ON" : u8"OFF"), COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnClose(popup.x + popup.width - 18, popup.y + popup.height - 4, 14, 3, u8"닫기", COLOR_WHITE, COLOR_YELLOW);

                btnToggleEffects.Update(input);
                btnToggleDebug.Update(input);
                btnToggleChar.Update(input);
                btnToggleAsc.Update(input);
                btnClose.Update(input);
                UpdateButtonAudio(btnToggleEffects, "title_settings_effects", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                UpdateButtonAudio(btnToggleDebug, "title_settings_debug", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                UpdateButtonAudio(btnToggleChar, "title_settings_char", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                UpdateButtonAudio(btnToggleAsc, "title_settings_asc", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                UpdateButtonAudio(btnClose, "title_settings_close", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);

                btnToggleEffects.Render(screen);
                btnToggleDebug.Render(screen);
                btnToggleChar.Render(screen);
                btnToggleAsc.Render(screen);
                btnClose.Render(screen);

                screen.DrawString(contentLeft, popup.y + popup.height - 7, u8"커스텀 시드 입력: 추후 구현 예정", FOREGROUND_INTENSITY);

                if (btnToggleEffects.IsClicked()) { settings.effectsEnabled = !settings.effectsEnabled; settingsDirty = true; }
                if (btnToggleDebug.IsClicked()) { settings.debugMode = !settings.debugMode; settingsDirty = true; }
                if (btnToggleChar.IsClicked()) { settings.enableCharacterSelect = !settings.enableCharacterSelect; settingsDirty = true; }
                if (btnToggleAsc.IsClicked()) { settings.enableAscensionSelect = !settings.enableAscensionSelect; settingsDirty = true; }
                if (btnClose.IsClicked() || pressedEsc) {
                    SaveManager::SaveSettings(settings);
                    settingsDirty = false;
                    titleOverlay = TitleOverlayType::None;
                    activeSliderId = -1;
                }
                else {
                    settingsDirty = true;
                }
            }
            else if (titleOverlay == TitleOverlayType::Records) {
                const Rect popup = { 10, 4, screen.GetWidth() - 20, screen.GetHeight() - 8 };
                RenderFrameBox(screen, popup, COLOR_YELLOW);
                RenderPanelTitle(screen, popup, u8"기록", COLOR_YELLOW);

                ButtonUI btnBack(popup.x + popup.width - 24, popup.y + popup.height - 5, 20, 4, u8"돌아가기", COLOR_WHITE, COLOR_YELLOW);
                btnBack.Update(input);
                UpdateButtonAudio(btnBack, "title_records_back", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                btnBack.Render(screen);

                if (pressedEsc || btnBack.IsClicked()) {
                    titleOverlay = TitleOverlayType::None;
                }

                if (!runRecords.empty()) {
                    const int listTop = popup.y + 5;
                    const int visibleCount = popup.height - 13;
                    if (input.GetWheelDelta() != 0) {
                        recordsScroll -= input.GetWheelDelta();
                    }
                    recordsScroll = (std::max)(0, (std::min)(recordsScroll, (std::max)(0, static_cast<int>(runRecords.size()) - visibleCount)));

                    const int listLeft = popup.x + 3;
                    const int listWidth = popup.width / 2 - 5;

                    for (int drawIndex = 0; drawIndex < visibleCount && (recordsScroll + drawIndex) < static_cast<int>(runRecords.size()); ++drawIndex) {
                        const int recordIndex = recordsScroll + drawIndex;
                        const Rect rowRect = { listLeft, listTop + drawIndex, listWidth, 1 };
                        const RunRecordData& record = runRecords[static_cast<size_t>(recordIndex)];
                        const string rowText =
                            string(record.won ? u8"[승]" : u8"[패]") + " " +
                            record.timestampText + " / " +
                            to_string(record.reachedFloor) + u8"층 / 시드 " + to_string(record.seed);

                        if (rowRect.Contains(mouseX, mouseY) && input.IsLeftClickDown()) {
                            selectedRecordIndex = recordIndex;
                        }

                        screen.DrawString(rowRect.x, rowRect.y, TextLayout::AlignToWidth(TextLayout::Utf8ToWide(rowText), rowRect.width, TextLayout::HorizontalAlign::Left), recordIndex == selectedRecordIndex ? COLOR_YELLOW : COLOR_WHITE);
                    }

                    selectedRecordIndex = (std::max)(0, (std::min)(selectedRecordIndex, static_cast<int>(runRecords.size()) - 1));
                    const RunRecordData& selectedRecord = runRecords[static_cast<size_t>(selectedRecordIndex)];

                    const Rect detailRect = { popup.x + popup.width / 2, popup.y + 1, popup.width / 2 - 3, popup.height - 9 };
                    RenderFrameBox(screen, detailRect, COLOR_WHITE);
                    RenderPanelTitle(screen, detailRect, selectedRecord.won ? u8"승리 기록" : u8"패배 기록", selectedRecord.won ? COLOR_GREEN : COLOR_RED);

                    const vector<string> detailLines = BuildRunRecordDetailLines(selectedRecord);

                    for (size_t index = 0; index < detailLines.size(); ++index) {
                        RenderWrappedText(screen, detailRect.x + 2, detailRect.y + 3 + static_cast<int>(index) * 2, detailRect.width - 4, detailLines[index], COLOR_WHITE);
                    }
                }

                const vector<string> statsLines = BuildStatsLines(globalStats);
                for (size_t index = 0; index < statsLines.size(); ++index) {
                    screen.DrawString(popup.x + 3, popup.y + popup.height - 2 - static_cast<int>(statsLines.size()) + static_cast<int>(index), statsLines[index], FOREGROUND_INTENSITY);
                }
            }
            else if (titleOverlay == TitleOverlayType::ConfirmAbandon) {
                const Rect popup = { screen.GetCenterX() - 22, screen.GetCenterY() - 5, 44, 11 };
                RenderFrameBox(screen, popup, COLOR_RED);
                RenderPanelTitle(screen, popup, u8"전투를 포기", COLOR_RED);
                RenderWrappedText(screen, popup.x + 3, popup.y + 4, popup.width - 6, u8"포기 시 패배로 처리됩니다. 정말 진행 중인 트라이를 포기하시겠습니까?", COLOR_WHITE);

                ButtonUI btnYes(popup.x + 7, popup.y + popup.height - 4, 12, 3, u8"네", COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnNo(popup.x + popup.width - 19, popup.y + popup.height - 4, 12, 3, u8"아니오", COLOR_WHITE, COLOR_YELLOW);
                btnYes.Update(input);
                btnNo.Update(input);
                UpdateButtonAudio(btnYes, "title_confirm_yes", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                UpdateButtonAudio(btnNo, "title_confirm_no", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                btnYes.Render(screen);
                btnNo.Render(screen);

                if (btnYes.IsClicked()) {
                    abandonContinueRunFromTitle();
                }
                if (btnNo.IsClicked() || pressedEsc) {
                    titleOverlay = TitleOverlayType::None;
                }
            }
            else if (titleOverlay == TitleOverlayType::CharacterSelect || titleOverlay == TitleOverlayType::AscensionSelect) {
                const bool characterOverlay = (titleOverlay == TitleOverlayType::CharacterSelect);
                const Rect popup = { screen.GetCenterX() - 24, screen.GetCenterY() - 7, 48, 14 };
                RenderFrameBox(screen, popup, COLOR_YELLOW);
                RenderPanelTitle(screen, popup, characterOverlay ? u8"캐릭터 선택 (임시)" : u8"승천 선택 (임시)", COLOR_YELLOW);
                RenderWrappedText(
                    screen,
                    popup.x + 3,
                    popup.y + 4,
                    popup.width - 6,
                    characterOverlay
                    ? u8"현재는 아이언클래드만 준비되어 있습니다. 이 토글이 켜져 있으면 시작 전에 이 임시 화면을 거칩니다."
                    : u8"승천 선택의 자리를 미리 확보한 화면입니다. 현재는 값 저장 없이 다음 단계로 넘어갑니다.",
                    COLOR_WHITE);

                ButtonUI btnContinue(popup.x + 5, popup.y + popup.height - 4, 16, 3, u8"계속", COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnBack(popup.x + popup.width - 21, popup.y + popup.height - 4, 16, 3, u8"뒤로", COLOR_WHITE, COLOR_YELLOW);
                btnContinue.Update(input);
                btnBack.Update(input);
                UpdateButtonAudio(btnContinue, "title_pick_continue", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                UpdateButtonAudio(btnBack, "title_pick_back", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                btnContinue.Render(screen);
                btnBack.Render(screen);

                if (btnContinue.IsClicked()) {
                    if (characterOverlay && settings.enableAscensionSelect) {
                        titleOverlay = TitleOverlayType::AscensionSelect;
                    }
                    else {
                        startNewRun();
                    }
                }
                if (btnBack.IsClicked() || pressedEsc) {
                    titleOverlay = TitleOverlayType::None;
                }
            }
        }
        else if (appState == AppState::Run) {
            const bool endingOverlayOpen = (run.overlay == RunOverlayType::Ending);
            const bool allowHudOverlayToggle =
                (run.overlay != RunOverlayType::Confirm &&
                 !endingOverlayOpen);

            if (pressedMapHotkey && allowHudOverlayToggle && run.scene != RunSceneType::CardPackSelect) {
                activeSliderId = -1;
                if (run.overlay == RunOverlayType::Map) {
                    run.overlay = RunOverlayType::None;
                    tooltip.SetVisible(false);
                }
                else {
                    run.overlay = RunOverlayType::Map;
                    tooltip.SetVisible(false);
                    mapRenderer.FocusToFloor(run.currentFloor > 0 ? run.currentFloor : 1);
                }
            }

            if (pressedEsc) {
                if (run.overlay == RunOverlayType::None) {
                    run.overlay = RunOverlayType::Settings;
                }
                else if (run.overlay == RunOverlayType::Settings || run.overlay == RunOverlayType::Map || run.overlay == RunOverlayType::Deck) {
                    run.overlay = RunOverlayType::None;
                    tooltip.SetVisible(false);
                    activeSliderId = -1;
                }
            }

            if (run.overlay != RunOverlayType::Map) {
                tooltip.SetVisible(false);
            }

            bool tooltipClaimedThisFrame = false;
            const auto showTooltipAtMouse = [&](const std::vector<std::string>& lines) {
                if (lines.empty()) {
                    return;
                }
                tooltip.SetText(lines);
                tooltip.SetVisible(true);
                tooltip.UpdatePosition(mouseX, mouseY, screen.GetWidth(), screen.GetHeight());
                tooltipClaimedThisFrame = true;
                };

            const string hpText = string(u8"체력: ") + to_string(run.player.currentHp) + "/" + to_string(run.player.maxHp);
            const string goldText = string(u8"골드: ") + to_string(run.gold);
            const string floorText = string(u8"층수: ") + to_string((std::max)(1, run.currentFloor));
            const string playTimeText = string(u8"플레이: ") + FormatPlayTime(run.playTimeSec);
            int hudCursorX = 2;
            screen.DrawString(hudCursorX, 1, hpText, COLOR_RED);
            hudCursorX += TextLayout::MeasureDisplayWidthUtf8(hpText) + 4;
            screen.DrawString(hudCursorX, 1, goldText, COLOR_YELLOW);
            hudCursorX += TextLayout::MeasureDisplayWidthUtf8(goldText) + 4;
            const int hudPotionStartX = hudCursorX;

            const string centerHudText = floorText + string("   ") + playTimeText;
            screen.DrawString(
                TextLayout::ComputeAlignedXUtf8(0, screen.GetWidth(), centerHudText, TextLayout::HorizontalAlign::Center),
                1,
                centerHudText,
                COLOR_WHITE);

            if (!(IsBattleNodeType(run.currentRoomType) && run.scene == RunSceneType::Room && run.overlay != RunOverlayType::Ending && !run.roomResolved)) {
                screen.DrawString(hudPotionStartX, 1, string(u8"포션: ") + BuildPotionListText(run.potions), FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
            }
            screen.DrawString(0, 3, string(static_cast<size_t>(screen.GetWidth()), '='), FOREGROUND_INTENSITY);

            const bool showRunNavigation = (run.scene != RunSceneType::CardPackSelect);
            const string deckButtonText = string(u8"덱(") + to_string(static_cast<int>(run.deck.size())) + ")";
            ButtonUI btnMap(screen.GetWidth() - 42, 0, 12, 3, u8"지도", COLOR_WHITE, COLOR_YELLOW);
            ButtonUI btnDeck(screen.GetWidth() - 29, 0, 12, 3, deckButtonText, COLOR_WHITE, COLOR_YELLOW);
            ButtonUI btnSettings(screen.GetWidth() - 16, 0, 12, 3, u8"설정", COLOR_WHITE, COLOR_YELLOW);

            if (allowHudOverlayToggle) {
                if (showRunNavigation) {
                    btnMap.Update(input);
                    btnDeck.Update(input);
                    UpdateButtonAudio(btnMap, "run_hud_map", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                    UpdateButtonAudio(btnDeck, "run_hud_deck", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                }
                btnSettings.Update(input);
                UpdateButtonAudio(btnSettings, "run_hud_settings", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);

                if (showRunNavigation && btnMap.IsClicked()) {
                    activeSliderId = -1;
                    if (run.overlay == RunOverlayType::Map) {
                        run.overlay = RunOverlayType::None;
                        tooltip.SetVisible(false);
                    }
                    else {
                        run.overlay = RunOverlayType::Map;
                        tooltip.SetVisible(false);
                        mapRenderer.FocusToFloor(run.currentFloor > 0 ? run.currentFloor : 1);
                    }
                }
                if (showRunNavigation && btnDeck.IsClicked()) {
                    activeSliderId = -1;
                    if (run.overlay == RunOverlayType::Deck) {
                        run.overlay = RunOverlayType::None;
                    }
                    else {
                        run.overlay = RunOverlayType::Deck;
                        tooltip.SetVisible(false);
                    }
                }
                if (btnSettings.IsClicked()) {
                    if (run.overlay == RunOverlayType::Settings) {
                        run.overlay = RunOverlayType::None;
                        activeSliderId = -1;
                    }
                    else {
                        activeSliderId = -1;
                        run.overlay = RunOverlayType::Settings;
                        tooltip.SetVisible(false);
                    }
                }
            }

            if (showRunNavigation) {
                btnMap.Render(screen);
                btnDeck.Render(screen);
            }
            btnSettings.Render(screen);

            if (run.scene == RunSceneType::CardPackSelect) {
                const bool cardPackInputAllowed = (run.overlay == RunOverlayType::None);
                if (!neowVoicePlayed) {
                    PlayRandomEffect(audio, neowVoicePool, L"vo_neow_intro", audioRng, audioAliasCounter, settings.effectsEnabled);
                    neowVoicePlayed = true;
                }
                const std::vector<int> offeredPackIndices = BuildStarterPackOfferIndices(run.seed, static_cast<int>(starterPacks.size()), 3);
                const std::vector<std::string>& playerCardPackArt = AsciiArtLibrary::Get(AsciiArtId::PlayerCardPack);
                const std::vector<std::string>& neowArt = AsciiArtLibrary::Get(AsciiArtId::Neow);
                const Rect playerArtClip = { 2, 7, 34, screen.GetHeight() - 12 };
                const Rect neowArtClip = { screen.GetWidth() - 72, 6, 160, screen.GetHeight() - 10 };
                const int playerArtBottomY = screen.GetHeight() - 5;
                const int neowArtBottomY = screen.GetHeight() - 4;
                const int neowCenterX = neowArtClip.x + (neowArtClip.width / 2) + kNeowCardPackOffsetX;
                const int packPanelWidth = 24;
                const int packPanelHeight = 18;
                const int packGap = 3;
                const int packTop = screen.GetHeight() - 22;
                const int packRowWidth = static_cast<int>(offeredPackIndices.size()) * packPanelWidth +
                    (static_cast<int>(offeredPackIndices.size()) - 1) * packGap;
                const int packStartX = screen.GetCenterX() - (packRowWidth / 2);
                const int detailBottomY = packTop - 2;
                const Rect headline = { screen.GetCenterX() - 32, 6, 64, 6 };
                const int expandedDetailWidth = screen.GetWidth() - (kCardPackExpandedPanelSideMargin * 2);
                const int expandedDetailTop = headline.y + headline.height + kCardPackExpandedPanelTopGap;
                const Rect collapsedDetailRect = { screen.GetCenterX() - 20, detailBottomY - 5, 40, 5 };
                const Rect expandedDetailRect = {
                    kCardPackExpandedPanelSideMargin,
                    expandedDetailTop,
                    expandedDetailWidth,
                    (std::max)(12, detailBottomY - expandedDetailTop + 1)
                };
                const Rect speechRect = { screen.GetWidth() - 49, 5, 22, 5 };

                RenderAnchoredArtClipped(screen, 19, playerArtBottomY, playerCardPackArt, COLOR_WHITE, playerArtClip);
                RenderAnchoredArtClipped(screen, neowCenterX, neowArtBottomY, neowArt, COLOR_WHITE, neowArtClip);

                RenderFrameBox(screen, speechRect, COLOR_WHITE);
                screen.DrawString(
                    speechRect.x + 1,
                    speechRect.y + 1,
                    TextLayout::AlignToWidth(TextLayout::Utf8ToWide(u8"선택하여라..."), speechRect.width - 2, TextLayout::HorizontalAlign::Center),
                    COLOR_WHITE);
                screen.DrawString(speechRect.x + speechRect.width - 7, speechRect.y + speechRect.height - 1, "\\", COLOR_WHITE);
                screen.DrawString(speechRect.x + speechRect.width - 6, speechRect.y + speechRect.height, " \\", COLOR_WHITE);

                RenderFrameBox(screen, headline, COLOR_YELLOW);
                screen.DrawString(
                    headline.x + 1,
                    headline.y + 1,
                    TextLayout::AlignToWidth(TextLayout::Utf8ToWide(u8"시너지 카드팩 선택"), headline.width - 2, TextLayout::HorizontalAlign::Center),
                    COLOR_YELLOW);
                RenderWrappedText(screen, headline.x + 3, headline.y + 2, headline.width - 6, u8"첫 시작에서는 랜덤한 3개 카드팩 중 하나를 고른 뒤 확인 버튼으로 런을 시작합니다.", COLOR_WHITE);

                std::vector<Rect> packRects;
                packRects.reserve(offeredPackIndices.size());
                int hoveredPackIndex = -1;
                for (size_t offerSlot = 0; offerSlot < offeredPackIndices.size(); ++offerSlot) {
                    const int packIndex = offeredPackIndices[offerSlot];
                    const Rect packRect = {
                        packStartX + static_cast<int>(offerSlot) * (packPanelWidth + packGap),
                        packTop,
                        packPanelWidth,
                        packPanelHeight
                    };
                    packRects.push_back(packRect);
                    const bool hovered = cardPackInputAllowed && packRect.Contains(mouseX, mouseY);
                    const bool selected = (run.selectedStarterPackIndex == packIndex);
                    const WORD frameColor = selected || hovered ? starterPacks[packIndex].accentColor : COLOR_WHITE;
                    if (hovered) {
                        hoveredPackIndex = packIndex;
                    }

                    RenderFrameBox(screen, packRect, frameColor);
                    RenderPanelTitle(screen, packRect, starterPacks[packIndex].title, frameColor);
                    RenderWrappedText(screen, packRect.x + 2, packRect.y + 4, packRect.width - 4, starterPacks[packIndex].description, COLOR_WHITE);

                    int drawY = packRect.y + 8;
                    for (const CardData& card : starterPacks[packIndex].cards) {
                        if (drawY >= packRect.y + packPanelHeight - 4) {
                            break;
                        }
                        screen.DrawString(packRect.x + 2, drawY, BuildCardSummary(card), COLOR_WHITE);
                        drawY += 2;
                    }

                    screen.DrawString(packRect.x + 2, packRect.y + packPanelHeight - 3, selected ? u8"선택됨" : u8"클릭하여 선택", selected ? frameColor : FOREGROUND_INTENSITY);
                    if (hovered && input.IsLeftClickDown()) {
                        run.selectedStarterPackIndex = packIndex;
                        animatedPackIndex = packIndex;
                        cardPackPanelProgress = (std::max)(cardPackPanelProgress, 0.35f);
                        PlayRandomEffect(audio, cardSelectPool, L"sfx_card_pack_select", audioRng, audioAliasCounter, settings.effectsEnabled);
                    }
                }

                const int targetPackIndex = (run.selectedStarterPackIndex >= 0) ? run.selectedStarterPackIndex : hoveredPackIndex;
                if (targetPackIndex >= 0 && animatedPackIndex != targetPackIndex && cardPackPanelProgress > 0.0f) {
                    animatedPackIndex = targetPackIndex;
                }
                if (animatedPackIndex < 0 && targetPackIndex >= 0) {
                    animatedPackIndex = targetPackIndex;
                }

                const float animationTarget = (targetPackIndex >= 0) ? 1.0f : 0.0f;
                const float animationStep = deltaTimeSec * 3.4f;
                if (cardPackPanelProgress < animationTarget) {
                    cardPackPanelProgress = (std::min)(animationTarget, cardPackPanelProgress + animationStep);
                }
                else if (cardPackPanelProgress > animationTarget) {
                    cardPackPanelProgress = (std::max)(animationTarget, cardPackPanelProgress - animationStep);
                }
                if (cardPackPanelProgress <= 0.001f && run.selectedStarterPackIndex < 0 && hoveredPackIndex < 0) {
                    animatedPackIndex = -1;
                }

                const int previewIndex = (run.selectedStarterPackIndex >= 0)
                    ? run.selectedStarterPackIndex
                    : animatedPackIndex;
                const float easedPanelProgress = EaseInOutCubic(cardPackPanelProgress);
                const Rect animatedDetailRect = LerpRectFixedBottomCenter(collapsedDetailRect, expandedDetailRect, easedPanelProgress);
                const WORD detailColor =
                    (previewIndex >= 0 && previewIndex < static_cast<int>(starterPacks.size()))
                    ? starterPacks[static_cast<size_t>(previewIndex)].accentColor
                    : COLOR_WHITE;

                RenderFrameBox(screen, animatedDetailRect, detailColor);

                if (previewIndex >= 0 && previewIndex < static_cast<int>(starterPacks.size())) {
                    const CardPackOption& previewPack = starterPacks[static_cast<size_t>(previewIndex)];
                    const int stableTitleX = TextLayout::ComputeAlignedXUtf8(
                        animatedDetailRect.x + 1,
                        animatedDetailRect.width - 2,
                        previewPack.title,
                        TextLayout::HorizontalAlign::Center);
                    screen.DrawString(stableTitleX, animatedDetailRect.y + 1, previewPack.title, detailColor);

                    const Rect packSummaryRect = { animatedDetailRect.x + 34, animatedDetailRect.y + 4, animatedDetailRect.width - 37, 5 };
                    const Rect cardDetailRect = { animatedDetailRect.x + 34, animatedDetailRect.y + 10, animatedDetailRect.width - 37, 8 };

                    std::vector<Rect> cardPreviewRects;
                    cardPreviewRects.reserve(previewPack.cards.size());
                    const int previewCardWidth = 16;
                    const int previewCardHeight = 6;
                    const int previewCardGapX = 3;
                    const int previewCardGapY = 1;
                    const int cardGridLeft = animatedDetailRect.x + 3;
                    const int cardGridTop = animatedDetailRect.y + 4;

                    for (size_t cardIndex = 0; cardIndex < previewPack.cards.size(); ++cardIndex) {
                        const int column = static_cast<int>(cardIndex % 2);
                        const int row = static_cast<int>(cardIndex / 2);
                        cardPreviewRects.push_back({
                            cardGridLeft + column * (previewCardWidth + previewCardGapX),
                            cardGridTop + row * (previewCardHeight + previewCardGapY),
                            previewCardWidth,
                            previewCardHeight
                            });
                    }

                    int hoveredPreviewCardIndex = -1;
                    for (size_t cardIndex = 0; cardIndex < cardPreviewRects.size(); ++cardIndex) {
                        const Rect& previewCardRect = cardPreviewRects[cardIndex];
                        if (!RectFitsInside(animatedDetailRect, previewCardRect)) {
                            continue;
                        }

                        const bool hoveredCard = previewCardRect.Contains(mouseX, mouseY);
                        if (hoveredCard) {
                            hoveredPreviewCardIndex = static_cast<int>(cardIndex);
                        }

                        RenderFrameBox(screen, previewCardRect, hoveredCard ? detailColor : COLOR_WHITE);
                        screen.DrawString(
                            previewCardRect.x + 1,
                            previewCardRect.y + 1,
                            TextLayout::AlignToWidth(TextLayout::Utf8ToWide(string("[") + to_string(previewPack.cards[cardIndex].cost) + "]"), previewCardRect.width - 2, TextLayout::HorizontalAlign::Left),
                            detailColor);
                        screen.DrawString(
                            previewCardRect.x + 1,
                            previewCardRect.y + 2,
                            TextLayout::AlignToWidth(TextLayout::Utf8ToWide(previewPack.cards[cardIndex].name), previewCardRect.width - 2, TextLayout::HorizontalAlign::Center),
                            hoveredCard ? detailColor : COLOR_WHITE);
                        screen.DrawString(
                            previewCardRect.x + 1,
                            previewCardRect.y + 4,
                            TextLayout::AlignToWidth(TextLayout::Utf8ToWide(BuildCardTypeText(previewPack.cards[cardIndex].type)), previewCardRect.width - 2, TextLayout::HorizontalAlign::Center),
                            FOREGROUND_INTENSITY);
                    }

                    if (RectFitsInside(animatedDetailRect, packSummaryRect)) {
                        std::string recommendedStyle = u8"균형 운영";
                        switch (previewPack.archetype) {
                        case CardArchetype::Combo: recommendedStyle = u8"공격 템포"; break;
                        case CardArchetype::Strength: recommendedStyle = u8"준비 후 폭발"; break;
                        case CardArchetype::Block: recommendedStyle = u8"방어 반격"; break;
                        case CardArchetype::Poison: recommendedStyle = u8"지속 압박"; break;
                        case CardArchetype::Cycle: recommendedStyle = u8"순환 가속"; break;
                        default: break;
                        }
                        const std::vector<std::string> summaryLines = {
                            previewPack.description,
                            string(u8"포함 카드 수: ") + to_string(previewPack.cards.size()),
                            string(u8"추천 성향: ") + recommendedStyle
                        };
                        RenderTextBlock(screen, { packSummaryRect.x + 1, packSummaryRect.y, packSummaryRect.width - 2, packSummaryRect.height }, summaryLines, COLOR_WHITE);
                    }

                    if (RectFitsInside(animatedDetailRect, cardDetailRect)) {
                        std::vector<std::string> detailLines;
                        if (hoveredPreviewCardIndex >= 0) {
                            const CardData& hoveredCard = previewPack.cards[static_cast<size_t>(hoveredPreviewCardIndex)];
                            detailLines.push_back(BuildCardSummary(hoveredCard));
                            detailLines.push_back(hoveredCard.description);
                        }
                        else {
                            detailLines.push_back(u8"카드를 올려두면 해당 카드의 설명이 이곳에 표시됩니다.");
                            detailLines.push_back(u8"선택된 카드팩은 시작 후 즉시 덱에 편입됩니다.");
                        }
                        RenderTextBlock(screen, { cardDetailRect.x + 1, cardDetailRect.y, cardDetailRect.width - 2, cardDetailRect.height }, detailLines, COLOR_WHITE);
                    }
                }
                else {
                    const int stableTitleX = TextLayout::ComputeAlignedXUtf8(
                        expandedDetailRect.x + 1,
                        expandedDetailRect.width - 2,
                        u8"선택 대기",
                        TextLayout::HorizontalAlign::Center);
                    screen.DrawString(stableTitleX, animatedDetailRect.y + 1, u8"선택 대기", COLOR_WHITE);
                    RenderWrappedText(screen, animatedDetailRect.x + 3, animatedDetailRect.y + 3, animatedDetailRect.width - 6, u8"카드팩 위에 마우스를 올리거나 클릭하면 상세 정보가 펼쳐집니다.", COLOR_WHITE);
                }

                ButtonUI btnConfirm(screen.GetCenterX() - 12, screen.GetHeight() - 4, 24, 3, u8"이 카드팩으로 시작", COLOR_WHITE, COLOR_YELLOW);
                if (run.selectedStarterPackIndex >= 0 && cardPackInputAllowed) {
                    btnConfirm.Update(input);
                    UpdateButtonAudio(btnConfirm, "cardpack_confirm", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                }
                btnConfirm.Render(screen);
                if (run.selectedStarterPackIndex < 0) {
                    screen.DrawString(screen.GetCenterX() + 14, screen.GetHeight() - 3, u8"먼저 카드팩을 선택하세요.", FOREGROUND_INTENSITY);
                }
                else if (btnConfirm.IsClicked()) {
                    ApplyStarterPack(run, starterPacks[static_cast<size_t>(run.selectedStarterPackIndex)]);
                    run.scene = RunSceneType::Room;
                    run.overlay = RunOverlayType::Map;
                    SaveManager::SaveContinueRun(run);
                    queueRoomBgm();
                }
            }
            else {
                const Rect roomPanel = { 18, 8, screen.GetWidth() - 36, screen.GetHeight() - 18 };

                string roomTitle;
                string roomBody;
                vector<string> artLines;
                bool showResolvedSummaryOnly = false;

                auto queueMapReturn = [&]() {
                    run.overlay = RunOverlayType::Map;
                    tooltip.SetVisible(false);
                    mapRenderer.FocusToFloor(run.currentFloor > 0 ? run.currentFloor : 1);
                    SaveManager::SaveContinueRun(run);
                    queueRoomBgm();
                    };

                const auto renderRewardPanel = [&](BattleRewardState& rewards, const Rect& rewardRect, const std::string& continueLabel) {
                    RenderFrameBox(screen, rewardRect, COLOR_YELLOW);
                    RenderPanelTitle(screen, rewardRect, rewards.title, COLOR_YELLOW);

                    bool continueRequested = false;

                    if (rewards.cardSelectionOpen && rewards.cardRewardAvailable && !rewards.cardRewardClaimed) {
                        const int cardTop = rewardRect.y + 5;
                        const int cardSpacing = 24;
                        for (size_t choiceIndex = 0; choiceIndex < rewards.cardChoices.size(); ++choiceIndex) {
                            Rect cardRect = { rewardRect.x + 4 + static_cast<int>(choiceIndex) * cardSpacing, cardTop, 22, 14 };
                            RenderFrameBox(screen, cardRect, cardRect.Contains(mouseX, mouseY) ? COLOR_YELLOW : COLOR_WHITE);
                            screen.DrawString(cardRect.x + 2, cardRect.y + 1, rewards.cardChoices[choiceIndex].name, COLOR_WHITE);
                            screen.DrawString(cardRect.x + 2, cardRect.y + 2, string(u8"코스트 ") + to_string(rewards.cardChoices[choiceIndex].cost), COLOR_YELLOW);
                            RenderWrappedText(screen, cardRect.x + 2, cardRect.y + 4, cardRect.width - 4, rewards.cardChoices[choiceIndex].description, COLOR_WHITE);

                            if (cardRect.Contains(mouseX, mouseY) && input.IsLeftClickDown() && run.overlay == RunOverlayType::None) {
                                run.deck.push_back(rewards.cardChoices[choiceIndex]);
                                rewards.cardRewardClaimed = true;
                                rewards.cardSelectionOpen = false;
                                rewards.cardChoices.clear();
                                SaveManager::SaveContinueRun(run);
                                PlayRandomEffect(audio, cardSelectPool, L"sfx_reward_card_pick", audioRng, audioAliasCounter, settings.effectsEnabled);
                                break;
                            }
                        }

                        ButtonUI btnSkip(rewardRect.x + rewardRect.width / 2 - 8, rewardRect.y + rewardRect.height - 4, 16, 3, u8"넘기기", COLOR_WHITE, COLOR_YELLOW);
                        if (run.overlay == RunOverlayType::None) {
                            btnSkip.Update(input);
                            UpdateButtonAudio(btnSkip, "reward_skip", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                        }
                        btnSkip.Render(screen);
                        if (btnSkip.IsClicked()) {
                            rewards.cardSelectionOpen = false;
                        }
                    }
                    else {
                        int rewardButtonY = rewardRect.y + 5;

                        if (rewards.goldAvailable && !rewards.goldClaimed) {
                            ButtonUI btnGold(rewardRect.x + 4, rewardButtonY, rewardRect.width - 8, 4, to_string(rewards.goldAmount) + u8" 골드", COLOR_WHITE, COLOR_YELLOW);
                            if (run.overlay == RunOverlayType::None) {
                                btnGold.Update(input);
                                UpdateButtonAudio(btnGold, "reward_gold", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                            }
                            btnGold.Render(screen);
                            if (btnGold.IsClicked()) {
                                run.gold += rewards.goldAmount;
                                rewards.goldClaimed = true;
                                SaveManager::SaveContinueRun(run);
                                PlayRandomEffect(audio, goldRewardPool, L"sfx_reward_gold", audioRng, audioAliasCounter, settings.effectsEnabled);
                            }
                            rewardButtonY += 5;
                        }

                        if (rewards.potionAvailable && !rewards.potionClaimed) {
                            ButtonUI btnPotion(rewardRect.x + 4, rewardButtonY, rewardRect.width - 8, 4, rewards.potion.name, COLOR_WHITE, COLOR_YELLOW);
                            if (run.overlay == RunOverlayType::None) {
                                btnPotion.Update(input);
                                UpdateButtonAudio(btnPotion, "reward_potion", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                            }
                            btnPotion.Render(screen);
                            screen.DrawString(rewardRect.x + 6, rewardButtonY + 1, BuildPotionActionText(rewards.potion), FOREGROUND_INTENSITY);
                            if (btnPotion.IsClicked()) {
                                if (HasPotionSlot(run)) {
                                    run.potions.push_back(rewards.potion);
                                    rewards.potionClaimed = true;
                                    SaveManager::SaveContinueRun(run);
                                    PlayRandomEffect(audio, potionDropPool, L"sfx_reward_potion", audioRng, audioAliasCounter, settings.effectsEnabled);
                                }
                            }
                            rewardButtonY += 5;
                        }

                        for (size_t relicIndex = 0; relicIndex < rewards.relicRewards.size(); ++relicIndex) {
                            const bool claimed = (relicIndex < rewards.relicClaimed.size()) && (rewards.relicClaimed[relicIndex] != 0);
                            if (claimed) {
                                continue;
                            }

                            ButtonUI btnRelic(rewardRect.x + 4, rewardButtonY, rewardRect.width - 8, 4, rewards.relicRewards[relicIndex].name, COLOR_WHITE, COLOR_YELLOW);
                            if (run.overlay == RunOverlayType::None) {
                                btnRelic.Update(input);
                                UpdateButtonAudio(btnRelic, "reward_relic_" + to_string(relicIndex), audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                            }
                            btnRelic.Render(screen);
                            if (btnRelic.IsHovered()) {
                                showTooltipAtMouse(BuildRelicTooltipLines(rewards.relicRewards[relicIndex]));
                            }
                            if (btnRelic.IsClicked()) {
                                run.relics.push_back(rewards.relicRewards[relicIndex]);
                                if (relicIndex >= rewards.relicClaimed.size()) {
                                    rewards.relicClaimed.resize(relicIndex + 1, 0);
                                }
                                rewards.relicClaimed[relicIndex] = 1;
                                SaveManager::SaveContinueRun(run);
                                PlayRandomEffect(audio, relicDropPool, L"sfx_reward_relic", audioRng, audioAliasCounter, settings.effectsEnabled);
                            }
                            rewardButtonY += 5;
                        }

                        if (rewards.cardRewardAvailable && !rewards.cardRewardClaimed) {
                            ButtonUI btnCard(rewardRect.x + 4, rewardButtonY, rewardRect.width - 8, 4, u8"덱에 카드를 추가", COLOR_WHITE, COLOR_YELLOW);
                            if (run.overlay == RunOverlayType::None) {
                                btnCard.Update(input);
                                UpdateButtonAudio(btnCard, "reward_card_open", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                            }
                            btnCard.Render(screen);
                            if (btnCard.IsClicked()) {
                                rewards.cardSelectionOpen = true;
                                PlayRandomEffect(audio, parchmentPool, L"sfx_reward_card_open", audioRng, audioAliasCounter, settings.effectsEnabled);
                            }
                            rewardButtonY += 5;
                        }

                        RenderWrappedText(screen, rewardRect.x + 4, rewardRect.y + rewardRect.height - 9, rewardRect.width - 8, rewards.message, COLOR_WHITE);
                        ButtonUI btnComplete(rewardRect.x + rewardRect.width / 2 - 10, rewardRect.y + rewardRect.height - 4, 20, 3, continueLabel, COLOR_WHITE, COLOR_YELLOW);
                        if (run.overlay == RunOverlayType::None) {
                            btnComplete.Update(input);
                            UpdateButtonAudio(btnComplete, "reward_complete", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                        }
                        btnComplete.Render(screen);
                        if (btnComplete.IsClicked()) {
                            continueRequested = true;
                        }
                    }

                    return continueRequested;
                };

                if (run.currentNodeId < 0) {
                    roomTitle = u8"다음 노드를 선택하세요";
                    roomBody = u8"지도 오버레이에서 현재 연결된 노드를 클릭하여 런을 진행합니다.";
                    artLines = { u8"<지도>", u8"연결된", u8"노드 클릭" };
                }
                else {
                    if (!run.roomResolved) {
                        PrepareCurrentRoomState(run);
                    }
                    roomTitle = RunNodeTypeToDisplayName(run.currentRoomType);
                    roomBody = RunNodeTypeToDescription(run.currentRoomType);

                    switch (run.currentRoomType) {
                    case RunNodeType::Battle:
                    case RunNodeType::Elite:
                    case RunNodeType::Boss:
                        roomBody = run.roomResolved && !run.currentRoomSummaryText.empty()
                            ? run.currentRoomSummaryText
                            : run.battleRoom.introText;
                        artLines = { u8"   /\\_/\\\\", u8"  ( 전투 )", u8"   >  <" };
                        break;
                    case RunNodeType::Shop:
                        roomBody += u8"\n카드 3장, 유물 1개, 포션 1개와 카드 제거 서비스를 제공합니다.";
                        artLines = { u8"  __________", u8" / 상  점 /|", u8"/______/ /", u8"|_____|/" };
                        break;
                    case RunNodeType::Rest:
                        roomBody += u8"\n휴식은 즉시 회복, 강화는 후속 단계 전용 자리만 먼저 확보합니다.";
                        artLines = { u8"    (  )", u8"   (    )", u8"  (불꽃 )", u8"    ||||" };
                        break;
                    case RunNodeType::Treasure:
                        roomBody += u8"\n유물, 골드, 포션 가운데 하나를 고른 뒤 결과를 확인합니다.";
                        artLines = { u8"   ______", u8"  /_____/\\", u8"  \\_____\\/" };
                        break;
                    case RunNodeType::Event:
                        roomBody = run.eventRoom.description;
                        artLines = run.eventRoom.artLines;
                        break;
                    }

                    if (run.roomResolved && !IsBattleNodeType(run.currentRoomType)) {
                        const bool runtimeMissing =
                            (run.currentRoomType == RunNodeType::Shop && !run.shopRoom.initialized) ||
                            (run.currentRoomType == RunNodeType::Rest && !run.restRoom.initialized) ||
                            (run.currentRoomType == RunNodeType::Treasure && !run.treasureRoom.initialized) ||
                            (run.currentRoomType == RunNodeType::Event && !run.eventRoom.initialized);

                        if (runtimeMissing) {
                            showResolvedSummaryOnly = true;
                            if (!run.currentRoomSummaryTitle.empty()) {
                                roomTitle = run.currentRoomSummaryTitle;
                            }
                            if (!run.currentRoomSummaryText.empty()) {
                                roomBody = run.currentRoomSummaryText;
                            }
                            artLines = { u8"<정리됨>" };
                        }
                    }
                }

                const bool showRoomFrame =
                    (run.currentNodeId < 0) ||
                    (!IsBattleNodeType(run.currentRoomType) &&
                        run.currentRoomType != RunNodeType::Shop &&
                        run.currentRoomType != RunNodeType::Rest &&
                        run.currentRoomType != RunNodeType::Treasure);
                if (showRoomFrame) {
                    RenderFrameBox(screen, roomPanel, COLOR_WHITE);
                    RenderPanelTitle(screen, roomPanel, roomTitle, COLOR_YELLOW);
                    const Rect artRect = { roomPanel.x + 3, roomPanel.y + 5, 22, roomPanel.height - 11 };
                    RenderFrameBox(screen, artRect, FOREGROUND_INTENSITY);
                    RenderAsciiArtLines(screen, artRect, artLines, COLOR_WHITE);
                    RenderWrappedText(screen, roomPanel.x + 28, roomPanel.y + 6, roomPanel.width - 32, roomBody, COLOR_WHITE);

                    screen.DrawString(roomPanel.x + 28, roomPanel.y + 13, string(u8"시드 ") + to_string(run.seed), FOREGROUND_INTENSITY);
                    screen.DrawString(roomPanel.x + 28, roomPanel.y + 15, string(u8"유물: ") + BuildRelicListText(run.relics), FOREGROUND_INTENSITY);
                    screen.DrawString(roomPanel.x + 28, roomPanel.y + 17, string(u8"포션: ") + BuildPotionListText(run.potions), FOREGROUND_INTENSITY);
                }

                if (run.currentNodeId < 0) {
                    ButtonUI btnOpenMap(roomPanel.x + 6, roomPanel.y + roomPanel.height - 6, 18, 3, u8"지도 열기", COLOR_WHITE, COLOR_YELLOW);
                    if (run.overlay == RunOverlayType::None) {
                        btnOpenMap.Update(input);
                    }
                    btnOpenMap.Render(screen);
                    if (btnOpenMap.IsClicked()) {
                        run.overlay = RunOverlayType::Map;
                        tooltip.SetVisible(false);
                        mapRenderer.FocusToFloor(1);
                    }
                }
                else if (showResolvedSummaryOnly) {
                    ButtonUI btnContinue(roomPanel.x + 28, roomPanel.y + roomPanel.height - 5, 18, 3, u8"계속", COLOR_WHITE, COLOR_YELLOW);
                    if (run.overlay == RunOverlayType::None) {
                        btnContinue.Update(input);
                    }
                    btnContinue.Render(screen);
                    if (btnContinue.IsClicked()) {
                        queueMapReturn();
                    }
                }
                else {
                    switch (run.currentRoomType) {
                    case RunNodeType::Battle:
                    case RunNodeType::Elite:
                    case RunNodeType::Boss:
                    {
                        auto applyBurningBlood = [&]() {
                            for (const RelicData& relic : run.relics) {
                                if (relic.name == u8"불타는 피") {
                                    run.player.currentHp = (std::min)(run.player.maxHp, run.player.currentHp + 6);
                                    break;
                                }
                            }
                        };

                        auto resolveBattleVictory = [&]() {
                            applyBurningBlood();
                            PlayFixedEffect(audio, L"sfx\\STS_BossVictoryStinger_1_v3_SFX.ogg", L"sfx_victory", audioAliasCounter, settings.effectsEnabled);

                            if (run.currentRoomType == RunNodeType::Boss) {
                                run.currentRoomSummaryTitle = u8"승리";
                                run.currentRoomSummaryText = u8"보스를 쓰러뜨렸습니다.";
                                ResolveCurrentNode(run, RunNodeResultType::Victory);
                                finishRunToEnding(true, "");
                                return;
                            }

                            run.currentRoomSummaryTitle = u8"전리품";
                            run.currentRoomSummaryText = u8"전투가 끝났습니다. 남은 보상을 챙기세요.";
                            run.battleRoom.rewards = BuildBattleRewardState(run, run.currentRoomType, RunNodeResultType::Victory);
                            ResolveCurrentNode(run, RunNodeResultType::Victory);
                            resetCombatPresentation();
                            SaveManager::SaveContinueRun(run);
                            queueRoomBgm();
                        };

                        auto resolveBattleEscape = [&]() {
                            run.currentRoomSummaryTitle = u8"전리품";
                            run.currentRoomSummaryText = u8"도망쳤다...";
                            run.battleRoom.rewards = BuildBattleRewardState(run, run.currentRoomType, RunNodeResultType::Escape);
                            ResolveCurrentNode(run, RunNodeResultType::Escape);
                            resetCombatPresentation();
                            SaveManager::SaveContinueRun(run);
                            queueRoomBgm();
                        };

                        auto resolveBattleDefeat = [&]() {
                            if (RunNodeState* currentNode = FindNodeById(run, run.currentNodeId)) {
                                currentNode->completed = true;
                                currentNode->result = RunNodeResultType::Defeat;
                                currentNode->isCurrent = false;
                            }
                            run.currentRoomResult = RunNodeResultType::Defeat;
                            run.roomResolved = true;
                            finishRunToEnding(false, to_string(run.currentFloor) + (run.currentRoomType == RunNodeType::Boss ? string(u8"층 보스전에서 쓰러졌습니다.") : string(u8"층 전투에서 쓰러졌습니다.")));
                        };

                        const int combatPlayerX = screen.GetWidth() * 20 / 100;
                        const int combatEnemyX = screen.GetWidth() * 82 / 100;
                        const int combatBaselineY = screen.GetHeight() - 24;
                        const int combatPlayerY = combatBaselineY;
                        const int combatEnemyY = combatBaselineY;
                        const Rect drawPileRect = { 2, screen.GetHeight() - 6, 10, 4 };
                        const Rect discardPileRect = { screen.GetWidth() - 12, screen.GetHeight() - 6, 10, 4 };
                        const Rect energyRect = { kCombatEnergyPanelX, screen.GetHeight() - 17, 28, 11 };
                        Rect intentRect = { combatEnemyX - 11, 8, 24, 5 };

                        auto renderDeadPlayerPose = [&]() {
                            static const std::vector<std::string>& deadLines = AsciiArtLibrary::Get(AsciiArtId::PlayerDeath);
                            RenderAnchoredArt(screen, combatPlayerX, combatPlayerY, deadLines, COLOR_RED);
                        };

                        if (run.overlay == RunOverlayType::Ending) {
                            if (endingStage == EndingStage::DefeatReveal || endingStage == EndingStage::DefeatRecord) {
                                if (enemyEntityUi) {
                                    enemyEntityUi->Render(screen);
                                }
                                renderDeadPlayerPose();
                            }
                            else {
                                if (playerEntityUi) {
                                    playerEntityUi->Render(screen);
                                }
                            }

                            if (endingStage == EndingStage::DefeatReveal) {
                                const Rect deathRect = { screen.GetCenterX() - 28, 8, 56, 6 };
                                RenderFrameBox(screen, deathRect, COLOR_RED);
                                screen.DrawString(
                                    deathRect.x + 2,
                                    deathRect.y + 2,
                                    TextLayout::AlignToWidth(TextLayout::Utf8ToWide(endingFlavorText), deathRect.width - 4, TextLayout::HorizontalAlign::Center),
                                    COLOR_RED);

                                ButtonUI btnContinue(screen.GetCenterX() - 8, screen.GetHeight() - 8, 16, 3, u8"계속", COLOR_WHITE, COLOR_YELLOW);
                                btnContinue.Update(input);
                                btnContinue.Render(screen);
                                if (btnContinue.IsClicked()) {
                                    endingStage = EndingStage::DefeatRecord;
                                }
                            }
                            else if (endingStage == EndingStage::DefeatRecord) {
                                const Rect recordRect = { screen.GetCenterX() - 34, 9, 68, 16 };
                                RenderFrameBox(screen, recordRect, COLOR_RED);
                                RenderPanelTitle(screen, recordRect, u8"패배 기록", COLOR_RED);
                                const std::vector<std::string> detailLines = BuildRunRecordDetailLines(endingRecord);
                                RenderTextBlock(
                                    screen,
                                    { recordRect.x + 3, recordRect.y + 3, recordRect.width - 6, recordRect.height - 7 },
                                    detailLines,
                                    COLOR_WHITE);

                                ButtonUI btnMenu(screen.GetCenterX() - 9, screen.GetHeight() - 8, 18, 3, u8"메인 메뉴", COLOR_WHITE, COLOR_YELLOW);
                                btnMenu.Update(input);
                                btnMenu.Render(screen);
                                if (btnMenu.IsClicked()) {
                                    transitionToTitle();
                                }
                            }
                            else if (endingStage == EndingStage::VictoryReveal) {
                                endingRevealProgress = (std::min)(1.0f, endingRevealProgress + (deltaTimeSec * 1.6f));
                                const int endX = screen.GetWidth() - 20;
                                const int startX = screen.GetWidth() + 6;
                                const Rect slideButton = { LerpInt(startX, endX, EaseOutCubic(endingRevealProgress)), screen.GetHeight() - 8, 16, 3 };
                                ButtonUI btnAdvance(slideButton.x, slideButton.y, slideButton.width, slideButton.height, u8"진행", COLOR_WHITE, COLOR_YELLOW);
                                if (endingRevealProgress >= 1.0f) {
                                    btnAdvance.Update(input);
                                }
                                btnAdvance.Render(screen);
                                if (btnAdvance.IsClicked()) {
                                    endingStage = EndingStage::VictorySummary;
                                }
                            }
                            else if (endingStage == EndingStage::VictorySummary) {
                                screen.DrawString(
                                    screen.GetCenterX() - 14,
                                    7,
                                    TextLayout::AlignToWidth(TextLayout::Utf8ToWide(u8"승리!"), 28, TextLayout::HorizontalAlign::Center),
                                    COLOR_GREEN);
                                const Rect victoryRect = { screen.GetCenterX() - 26, 10, 52, 6 };
                                RenderFrameBox(screen, victoryRect, COLOR_GREEN);
                                RenderWrappedText(screen, victoryRect.x + 3, victoryRect.y + 2, victoryRect.width - 6, u8"첨탑의 정점에 도달했습니다.\n다음 연출 자리는 비워두었습니다.", COLOR_WHITE);

                                ButtonUI btnContinue(screen.GetCenterX() - 8, screen.GetHeight() - 8, 16, 3, u8"진행", COLOR_WHITE, COLOR_YELLOW);
                                btnContinue.Update(input);
                                btnContinue.Render(screen);
                                if (btnContinue.IsClicked()) {
                                    transitionToTitle();
                                }
                            }
                            break;
                        }

                        if (!run.roomResolved) {
                            const int roomKey = (run.currentNodeId >= 0) ? run.currentNodeId : (-100 - static_cast<int>(run.currentRoomType));
                            if (!combatSystem || activeCombatRoomKey != roomKey) {
                                resetCombatPresentation();
                                if (!run.battleRoom.initialized) {
                                    PrepareCurrentRoomState(run);
                                }

                                combatSystem = std::make_unique<CombatSystem>(&run.player, &run.battleRoom.enemy);
                                CombatConfig& config = combatSystem->GetMutableConfig();
                                if (run.currentRoomType == RunNodeType::Elite) {
                                    config.baseEnemyIntentIntervalSec = 5.0f;
                                    config.minEnemyIntentIntervalSec = 1.8f;
                                    config.speedGainPerEnemyAction = 0.06f;
                                }
                                else if (run.currentRoomType == RunNodeType::Boss) {
                                    config.baseEnemyIntentIntervalSec = 4.7f;
                                    config.minEnemyIntentIntervalSec = 1.6f;
                                    config.speedGainPerEnemyAction = 0.07f;
                                }
                                combatSystem->StartBattle(run.deck);

                                playerEntityUi = std::make_unique<EntityUI>(combatPlayerX, combatPlayerY, &run.player, true);
                                enemyEntityUi = std::make_unique<EntityUI>(combatEnemyX, combatEnemyY, &run.battleRoom.enemy, false);
                                activeCombatRoomKey = roomKey;
                                queueRoomBgm();
                            }

                            if (playerEntityUi) {
                                playerEntityUi->SetAnchorBottomCenter(combatPlayerX, combatPlayerY);
                            }
                            if (enemyEntityUi) {
                                enemyEntityUi->SetAnchorBottomCenter(combatEnemyX, combatEnemyY);
                                intentRect.x = enemyEntityUi->GetArtCenterX() - (intentRect.width / 2);
                                intentRect.y = enemyEntityUi->GetArtTopY() - intentRect.height - 3;
                                intentRect.x = (std::max)(2, (std::min)(screen.GetWidth() - intentRect.width - 2, intentRect.x));
                                intentRect.y = (std::max)(5, intentRect.y);
                            }

                            vector<ButtonUI> potionButtons;
                            for (size_t potionIndex = 0; potionIndex < run.potions.size(); ++potionIndex) {
                                potionButtons.emplace_back(
                                    hudPotionStartX + static_cast<int>(potionIndex) * 16,
                                    0,
                                    16,
                                    3,
                                    run.potions[potionIndex].name,
                                    COLOR_WHITE,
                                    COLOR_YELLOW);
                            }

                            const auto& hand = combatSystem->GetHand();
                            const int cardCount = static_cast<int>(hand.size());
                            const int cardWidth = CardUI::DefaultWidth();
                            const int overlapChars = cardCount >= 7 ? 12 : (cardCount >= 5 ? 8 : (cardCount >= 2 ? 2 : 0));
                            const int cardSpacing = cardWidth - overlapChars;
                            const int totalHandWidth = cardCount > 0 ? (cardWidth + (cardCount - 1) * cardSpacing) : 0;
                            const int handStartX = (screen.GetWidth() - totalHandWidth) / 2;
                            const int handBaseY = screen.GetHeight() - 15 - kCombatHandRaiseRows;
                            const bool allowCardInteraction = (run.overlay == RunOverlayType::None);

                            const auto recenterHandLayout = [&]() {
                                handLayoutXs.clear();
                                handLayoutXs.reserve(static_cast<size_t>(cardCount));
                                for (int cardIndex = 0; cardIndex < cardCount; ++cardIndex) {
                                    handLayoutXs.push_back(handStartX + cardIndex * cardSpacing);
                                }
                                nextDrawCardGoesRight = true;
                                forceRecenterHandLayout = false;
                                };

                            if (cardCount <= 0) {
                                handLayoutXs.clear();
                                nextDrawCardGoesRight = true;
                                forceRecenterHandLayout = true;
                            }
                            else if (forceRecenterHandLayout || handLayoutXs.empty() || static_cast<int>(handLayoutXs.size()) > cardCount) {
                                recenterHandLayout();
                            }
                            else {
                                while (static_cast<int>(handLayoutXs.size()) < cardCount) {
                                    const auto minMaxX = std::minmax_element(handLayoutXs.begin(), handLayoutXs.end());
                                    const int addX = nextDrawCardGoesRight
                                        ? (*minMaxX.second + cardSpacing)
                                        : (*minMaxX.first - cardSpacing);
                                    handLayoutXs.push_back(addX);
                                    nextDrawCardGoesRight = !nextDrawCardGoesRight;
                                }

                                const auto minMaxX = std::minmax_element(handLayoutXs.begin(), handLayoutXs.end());
                                if (*minMaxX.first < 2 || *minMaxX.second + cardWidth > screen.GetWidth() - 2) {
                                    recenterHandLayout();
                                }
                            }

                            vector<CardUI> handCards;
                            handCards.reserve(hand.size());
                            for (size_t cardIndex = 0; cardIndex < hand.size(); ++cardIndex) {
                                const int cardX = handLayoutXs[cardIndex];
                                handCards.emplace_back(cardX, handBaseY, const_cast<CardData*>(&hand[cardIndex]));
                                handCards.back().SetBasePosition(cardX, handBaseY);
                                handCards.back().SetRightOcclusion(cardIndex + 1 < hand.size() ? overlapChars : 0);
                                handCards.back().SetFrameColor(GetCardTypeFrameColor(hand[cardIndex].type));
                                handCards.back().SetPlayable(hand[cardIndex].cost <= combatSystem->GetEnergy());
                            }

                            int hoveredHandIndex = -1;
                            for (int cardIndex = cardCount - 1; cardIndex >= 0; --cardIndex) {
                                if (handCards[static_cast<size_t>(cardIndex)].IsPointInside(mouseX, mouseY)) {
                                    hoveredHandIndex = cardIndex;
                                    break;
                                }
                            }

                            for (int cardIndex = 0; cardIndex < cardCount; ++cardIndex) {
                                CardUI& cardUi = handCards[static_cast<size_t>(cardIndex)];
                                const bool hasEnergy = hand[static_cast<size_t>(cardIndex)].cost <= combatSystem->GetEnergy();
                                const bool hovered = allowCardInteraction && hasEnergy && (draggedHandIndex < 0 && cardIndex == hoveredHandIndex);
                                cardUi.SetHovered(hovered || cardIndex == draggedHandIndex);
                                if ((hovered || cardIndex == draggedHandIndex) && hasEnergy) {
                                    cardUi.SetPosition(cardUi.GetX(), handBaseY - 3);
                                }
                            }

                            if (draggedHandIndex >= cardCount) {
                                draggedHandIndex = -1;
                                targetingArrow.SetActive(false);
                            }
                            if (!allowCardInteraction && draggedHandIndex >= 0) {
                                draggedHandIndex = -1;
                                targetingArrow.SetActive(false);
                            }

                            if (draggedHandIndex < 0 && input.IsLeftClickDown() && hoveredHandIndex >= 0 && run.overlay == RunOverlayType::None) {
                                draggedHandIndex = hoveredHandIndex;
                            }

                            CombatDropTarget dropTarget = CombatDropTarget::None;
                            const Rect expandedDrawRect = {
                                drawPileRect.x,
                                drawPileRect.y - 8,
                                drawPileRect.width + 14,
                                drawPileRect.height + 8
                            };
                            const Rect expandedDiscardRect = {
                                discardPileRect.x - 26,
                                discardPileRect.y - 14,
                                discardPileRect.width + 26,
                                discardPileRect.height + 14
                            };
                            const bool canManualDraw = allowCardInteraction &&
                                draggedHandIndex < 0 &&
                                combatSystem->GetManualDrawCharges() > 0;
                            const bool drawPileHovered = canManualDraw && expandedDrawRect.Contains(mouseX, mouseY);
                            if (draggedHandIndex >= 0 && draggedHandIndex < cardCount) {
                                const CardData& draggedCard = hand[static_cast<size_t>(draggedHandIndex)];
                                const bool hasEnergy = draggedCard.cost <= combatSystem->GetEnergy();
                                if (expandedDiscardRect.Contains(mouseX, mouseY)) {
                                    dropTarget = CombatDropTarget::DiscardPile;
                                }
                                else if (hasEnergy && draggedCard.targetType == CardTargetType::Enemy && enemyEntityUi && enemyEntityUi->IsPointInside(mouseX, mouseY)) {
                                    dropTarget = CombatDropTarget::Enemy;
                                }
                                else if (hasEnergy && draggedCard.type == CardType::Attack && draggedCard.targetType == CardTargetType::Self && playerEntityUi && playerEntityUi->IsPointInside(mouseX, mouseY)) {
                                    dropTarget = CombatDropTarget::Player;
                                }

                                const bool showArrow = hasEnergy || dropTarget == CombatDropTarget::DiscardPile;
                                targetingArrow.SetActive(showArrow);
                                targetingArrow.SetColor(dropTarget == CombatDropTarget::DiscardPile
                                    ? COLOR_WHITE
                                    : (draggedCard.type == CardType::Attack ? COLOR_RED : FOREGROUND_INTENSITY));
                                targetingArrow.SetStartPoint(handCards[static_cast<size_t>(draggedHandIndex)].GetX() + (cardWidth / 2), handBaseY - 1);
                                targetingArrow.SetEndPoint(mouseX, mouseY);
                            }
                            else {
                                targetingArrow.SetActive(false);
                            }

                            const float discardTargetProgress = (dropTarget == CombatDropTarget::DiscardPile) ? 1.0f : 0.0f;
                            const float discardAnimSpeed = deltaTimeSec * 12.0f;
                            if (discardPileExpandProgress < discardTargetProgress) {
                                discardPileExpandProgress = (std::min)(discardTargetProgress, discardPileExpandProgress + discardAnimSpeed);
                            }
                            else if (discardPileExpandProgress > discardTargetProgress) {
                                discardPileExpandProgress = (std::max)(discardTargetProgress, discardPileExpandProgress - discardAnimSpeed);
                            }
                            const float drawTargetProgress = drawPileHovered ? 1.0f : 0.0f;
                            if (drawPileExpandProgress < drawTargetProgress) {
                                drawPileExpandProgress = (std::min)(drawTargetProgress, drawPileExpandProgress + discardAnimSpeed);
                            }
                            else if (drawPileExpandProgress > drawTargetProgress) {
                                drawPileExpandProgress = (std::max)(drawTargetProgress, drawPileExpandProgress - discardAnimSpeed);
                            }
                            const Rect drawRenderRect = LerpRectFixedBottomLeft(drawPileRect, expandedDrawRect, EaseOutCubic(drawPileExpandProgress));
                            const Rect discardRenderRect = LerpRectFixedBottomRight(discardPileRect, expandedDiscardRect, EaseOutCubic(discardPileExpandProgress));

                            if (playerEntityUi) {
                                playerEntityUi->SetTargeted(dropTarget == CombatDropTarget::Player);
                                playerEntityUi->Update(input);
                            }
                            if (enemyEntityUi) {
                                enemyEntityUi->SetTargeted(dropTarget == CombatDropTarget::Enemy);
                                enemyEntityUi->Update(input);
                            }

                            if (run.overlay == RunOverlayType::None) {
                                for (ButtonUI& potionButton : potionButtons) {
                                    potionButton.Update(input);
                                }
                            }

                            bool escapeRequested = false;
                            for (size_t potionIndex = 0; potionIndex < potionButtons.size() && potionIndex < run.potions.size(); ++potionIndex) {
                                UpdateRectHoverAudio(potionButtons[potionIndex].IsHovered(), "combat_potion_" + to_string(potionIndex), audio, potionUsePool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                                potionButtons[potionIndex].Render(screen);
                                if (potionButtons[potionIndex].IsClicked()) {
                                    const PotionData potion = run.potions[potionIndex];
                                    if (potion.name == u8"회복 포션") {
                                        run.player.currentHp = (std::min)(run.player.maxHp, run.player.currentHp + 12);
                                        PlayRandomEffect(audio, potionUsePool, L"sfx_potion_use", audioRng, audioAliasCounter, settings.effectsEnabled);
                                        run.potions.erase(run.potions.begin() + static_cast<vector<PotionData>::difference_type>(potionIndex));
                                        break;
                                    }
                                    if (potion.name == u8"에너지 포션") {
                                        combatSystem->AddEnergy(2);
                                        PlayRandomEffect(audio, potionUsePool, L"sfx_potion_use", audioRng, audioAliasCounter, settings.effectsEnabled);
                                        run.potions.erase(run.potions.begin() + static_cast<vector<PotionData>::difference_type>(potionIndex));
                                        break;
                                    }
                                    if (potion.name == u8"연막 포션" && run.currentRoomType != RunNodeType::Boss) {
                                        PlayRandomEffect(audio, potionUsePool, L"sfx_potion_use", audioRng, audioAliasCounter, settings.effectsEnabled);
                                        run.potions.erase(run.potions.begin() + static_cast<vector<PotionData>::difference_type>(potionIndex));
                                        escapeRequested = true;
                                        break;
                                    }
                                }
                            }

                            if (drawPileHovered && input.IsLeftClickDown()) {
                                const CombatActionResult drawResult = combatSystem->TryManualDrawFromPile();
                                if (drawResult.success) {
                                    PlayFixedEffect(audio, L"card_pick_sfx.wav", L"sfx_draw", audioAliasCounter, settings.effectsEnabled);
                                }
                                else {
                                    PlayRandomEffect(audio, cardRejectPool, L"sfx_draw_reject", audioRng, audioAliasCounter, settings.effectsEnabled);
                                }
                            }

                            if (draggedHandIndex >= 0 && input.IsLeftClickUp() && run.overlay == RunOverlayType::None) {
                                bool actionSucceeded = false;
                                if (dropTarget == CombatDropTarget::DiscardPile) {
                                    CombatActionResult actionResult = combatSystem->TryDiscardCard(draggedHandIndex);
                                    actionSucceeded = actionResult.success;
                                    if (actionResult.success) {
                                        ++globalStats.totalCardsDiscarded;
                                        forceRecenterHandLayout = true;
                                        if (enemyEntityUi && actionResult.enemyHit) {
                                            enemyEntityUi->TriggerHitAnimation();
                                        }
                                        PlayRandomEffect(audio, cardRejectPool, L"sfx_discard", audioRng, audioAliasCounter, settings.effectsEnabled);
                                    }
                                }
                                else if (dropTarget == CombatDropTarget::Enemy || dropTarget == CombatDropTarget::Player || IsGroundUsableCard(hand[static_cast<size_t>(draggedHandIndex)])) {
                                    const CardData playedCard = hand[static_cast<size_t>(draggedHandIndex)];
                                    const EntityData playerBeforeAction = run.player;
                                    const EntityData enemyBeforeAction = run.battleRoom.enemy;
                                    CombatDropTarget actionTarget = dropTarget;
                                    if (actionTarget == CombatDropTarget::None && IsGroundUsableCard(hand[static_cast<size_t>(draggedHandIndex)])) {
                                        actionTarget = CombatDropTarget::None;
                                    }
                                    CombatActionResult actionResult = combatSystem->TryUseCard(draggedHandIndex, actionTarget);
                                    actionSucceeded = actionResult.success;
                                    if (actionResult.success) {
                                        ++globalStats.totalCardsUsed;
                                        forceRecenterHandLayout = true;
                                        if (actionResult.usedPowerCard) {
                                            powerBannerText = BuildShortPowerBannerText(actionResult.usedCardName);
                                            powerBannerTimerSec = kPowerBannerDurationSec;
                                        }
                                        if (enemyEntityUi && actionResult.enemyHit) {
                                            enemyEntityUi->TriggerHitAnimation();
                                        }
                                        if (playerEntityUi && actionResult.playerHit) {
                                            playerEntityUi->TriggerHitAnimation();
                                        }
                                        if (playedCard.type == CardType::Power) {
                                            PlayRandomEffect(audio, buffPool, L"sfx_power_card", audioRng, audioAliasCounter, settings.effectsEnabled);
                                        }
                                        else if (playedCard.type == CardType::Attack) {
                                            if (IsFireAttackCard(playedCard)) {
                                                PlayRandomEffect(audio, { L"sfx\\SOTE_SFX_FireIgnite_1_v1.ogg", L"sfx\\SOTE_SFX_FireIgnite_2_v1.ogg" }, L"sfx_attack_fire", audioRng, audioAliasCounter, settings.effectsEnabled);
                                            }
                                            else if (IsThunderCard(playedCard)) {
                                                PlayFixedEffect(audio, L"sfx\\SOTE_SFX_ThunderclapCard_v1.ogg", L"sfx_attack_thunder", audioAliasCounter, settings.effectsEnabled);
                                            }
                                            else if (IsSpecialAttackCard(playedCard)) {
                                                PlayRandomEffect(audio, { L"sfx\\SOTE_SFX_IronClad_Atk_RR1_v2.ogg", L"sfx\\SOTE_SFX_IronClad_Atk_RR2_v2.ogg", L"sfx\\SOTE_SFX_IronClad_Atk_RR3_v2.ogg" }, L"sfx_attack_special", audioRng, audioAliasCounter, settings.effectsEnabled);
                                            }
                                            else if (IsHeavyAttackCard(playedCard)) {
                                                PlayRandomEffect(audio, { L"sfx\\SOTE_SFX_HeavyAtk_v2.ogg", L"sfx\\SOTE_SFX_HeavyBlunt_v2.ogg" }, L"sfx_attack_heavy", audioRng, audioAliasCounter, settings.effectsEnabled);
                                            }
                                            else {
                                                PlayFixedEffect(audio, L"sfx\\SOTE_SFX_FastBlunt_v2.ogg", L"sfx_attack_fast", audioAliasCounter, settings.effectsEnabled);
                                            }
                                        }
                                        if (run.player.block > playerBeforeAction.block) {
                                            PlayRandomEffect(audio, gainDefensePool, L"sfx_gain_block", audioRng, audioAliasCounter, settings.effectsEnabled);
                                        }
                                        if (run.player.strength > playerBeforeAction.strength) {
                                            PlayFixedEffect(audio, L"sfx\\STS_SFX_Strength_v1.ogg", L"sfx_strength_up", audioAliasCounter, settings.effectsEnabled);
                                        }
                                        if (run.player.dexterity > playerBeforeAction.dexterity) {
                                            PlayFixedEffect(audio, L"sfx\\STS_SFX_Dexterity_v2.ogg", L"sfx_dexterity_up", audioAliasCounter, settings.effectsEnabled);
                                        }
                                        if (run.player.vulnerable > playerBeforeAction.vulnerable ||
                                            run.player.weak > playerBeforeAction.weak ||
                                            run.player.poison > playerBeforeAction.poison ||
                                            run.battleRoom.enemy.vulnerable > enemyBeforeAction.vulnerable ||
                                            run.battleRoom.enemy.weak > enemyBeforeAction.weak ||
                                            run.battleRoom.enemy.poison > enemyBeforeAction.poison) {
                                            PlayRandomEffect(audio, debuffPool, L"sfx_debuff_apply", audioRng, audioAliasCounter, settings.effectsEnabled);
                                        }
                                        if (playedCard.exhausts) {
                                            PlayFixedEffect(audio, L"sfx\\SOTE_SFX_ExhaustCard.ogg", L"sfx_card_exhaust", audioAliasCounter, settings.effectsEnabled);
                                        }
                                    }
                                    else {
                                        PlayRandomEffect(audio, cardRejectPool, L"sfx_card_reject", audioRng, audioAliasCounter, settings.effectsEnabled);
                                    }
                                }

                                if (!actionSucceeded && dropTarget == CombatDropTarget::DiscardPile) {
                                    PlayRandomEffect(audio, cardRejectPool, L"sfx_discard", audioRng, audioAliasCounter, settings.effectsEnabled);
                                }

                                draggedHandIndex = -1;
                                targetingArrow.SetActive(false);
                            }

                            const bool allowCombatTime = (run.overlay == RunOverlayType::None);
                            const float combatTimeScale = allowCombatTime
                                ? (draggedHandIndex >= 0 ? combatSystem->GetDragTimeScale() : 1.0f)
                                : 0.0f;
                            const CombatFrameResult frameResult = combatSystem->Update(deltaTimeSec, combatTimeScale * (settings.gameSpeedPercent / 100.0f));
                            powerBannerTimerSec = (std::max)(0.0f, powerBannerTimerSec - deltaTimeSec);

                            if (frameResult.enemyHit && enemyEntityUi) {
                                enemyEntityUi->TriggerHitAnimation();
                            }
                            if (frameResult.playerHit && playerEntityUi) {
                                playerEntityUi->TriggerHitAnimation();
                            }
                            if (frameResult.overdrawRejected) {
                                PlayRandomEffect(audio, cardRejectPool, L"sfx_overdraw", audioRng, audioAliasCounter, settings.effectsEnabled);
                            }
                            if (frameResult.playerAttackBlocked) {
                                PlayFixedEffect(audio, L"sfx\\SOTE_SFX_BlockAtk_v2.ogg", L"sfx_player_blocked", audioAliasCounter, settings.effectsEnabled);
                            }
                            if (frameResult.playerBlockBroken) {
                                PlayFixedEffect(audio, L"sfx\\SOTE_SFX_DefenseBreak_v2.ogg", L"sfx_player_block_break", audioAliasCounter, settings.effectsEnabled);
                            }
                            if (frameResult.enemyBlockGained > 0) {
                                PlayRandomEffect(audio, gainDefensePool, L"sfx_enemy_block_gain", audioRng, audioAliasCounter, settings.effectsEnabled);
                            }
                            if (frameResult.enemyStrengthGained > 0) {
                                PlayRandomEffect(audio, buffPool, L"sfx_enemy_buff", audioRng, audioAliasCounter, settings.effectsEnabled);
                            }

                            const WORD drawPileColor = drawPileHovered ? COLOR_YELLOW : COLOR_WHITE;
                            RenderFrameBox(screen, drawRenderRect, drawPileColor);
                            screen.DrawString(drawRenderRect.x + 2, drawRenderRect.y + 1, u8"뽑기", drawPileColor);
                            screen.DrawString(drawRenderRect.x + 2, drawRenderRect.y + 2, string("Count ") + to_string(combatSystem->GetDrawPileCount()), COLOR_WHITE);
                            if (combatSystem->GetManualDrawCharges() > 0 && drawRenderRect.height >= 5) {
                                screen.DrawString(drawRenderRect.x + 2, drawRenderRect.y + 3, string(u8"성급 ") + to_string(combatSystem->GetManualDrawCharges()), drawPileColor);
                            }

                            RenderFrameBox(screen, discardRenderRect, dropTarget == CombatDropTarget::DiscardPile ? COLOR_YELLOW : COLOR_WHITE);
                            screen.DrawString(discardRenderRect.x + 2, discardRenderRect.y + 1, u8"버리기", dropTarget == CombatDropTarget::DiscardPile ? COLOR_YELLOW : COLOR_WHITE);
                            screen.DrawString(discardRenderRect.x + 2, discardRenderRect.y + 2, string("Count ") + to_string(combatSystem->GetDiscardPileCount()), COLOR_WHITE);

                            RenderBigEnergy(screen, energyRect, combatSystem->GetEnergy(), combatSystem->GetMaxEnergy(), COLOR_YELLOW);
                            screen.DrawString(energyRect.x + 2, energyRect.y + energyRect.height - 1, string(u8"속도 x") + FormatFloat(combatSystem->GetSpeedMultiplier(), 2), COLOR_GREEN);

                            RenderFrameBox(screen, intentRect, COLOR_RED);
                            const EnemyIntentState& intent = combatSystem->GetCurrentIntent();
                            const string intentHeader = intent.type == EnemyIntentType::Attack
                                ? u8"[ATK] 공격 예고"
                                : (intent.type == EnemyIntentType::Defend ? u8"[BUF] 강화 예고" : u8"[BUF] 강화 예고");
                            screen.DrawString(intentRect.x + 2, intentRect.y + 1, intentHeader, COLOR_RED);
                            screen.DrawString(intentRect.x + 2, intentRect.y + 2, string(u8"피해/수치 ") + to_string(intent.value) + "  " + FormatFloat(combatSystem->GetEnemyIntentRemainingSec(), 1) + "s", COLOR_WHITE);
                            const int gaugeFill = static_cast<int>(std::round((intentRect.width - 4) * combatSystem->GetEnemyIntentProgress01()));
                            string gauge(static_cast<size_t>(intentRect.width - 4), '.');
                            for (int fillIndex = 0; fillIndex < gaugeFill && fillIndex < static_cast<int>(gauge.size()); ++fillIndex) {
                                gauge[static_cast<size_t>(fillIndex)] = '#';
                            }
                            screen.DrawString(intentRect.x + 2, intentRect.y + 3, "[" + gauge + "]", COLOR_WHITE);

                            if (playerEntityUi) {
                                playerEntityUi->Render(screen);
                            }
                            if (enemyEntityUi) {
                                enemyEntityUi->Render(screen);
                            }

                            const auto renderEntityStatusHud = [&](EntityUI* entityUi, const EntityData& entityData) {
                                if (entityUi == nullptr) {
                                    return;
                                }

                                const int barX = entityUi->GetHealthBarX();
                                const int barY = entityUi->GetHealthBarY();
                                const int barWidth = entityUi->GetHealthBarWidth();
                                const bool inputEnabled = (run.overlay == RunOverlayType::None);

                                if (entityData.poison > 0) {
                                    const std::string poisonText = std::string(u8"독") + std::to_string(entityData.poison);
                                    const int poisonX = barX + barWidth + 2;
                                    const Rect poisonRect = { poisonX, barY, TextLayout::MeasureDisplayWidthUtf8(poisonText), 1 };
                                    screen.DrawString(poisonX, barY, poisonText, COLOR_GREEN);
                                    if (inputEnabled && poisonRect.Contains(mouseX, mouseY)) {
                                        showTooltipAtMouse(BuildStatusTooltipLines(u8"독", entityData.poison));
                                    }
                                }

                                int nextStatusLineY = barY + 1;
                                if (entityData.block > 0) {
                                    const std::string blockText = std::string(u8"방어도 ") + std::to_string(entityData.block);
                                    const int blockX = TextLayout::ComputeAlignedXUtf8(barX, barWidth, blockText, TextLayout::HorizontalAlign::Center);
                                    const Rect blockRect = { blockX, nextStatusLineY, TextLayout::MeasureDisplayWidthUtf8(blockText), 1 };
                                    screen.DrawString(blockX, nextStatusLineY, blockText, COLOR_BLUE);
                                    if (inputEnabled && blockRect.Contains(mouseX, mouseY)) {
                                        showTooltipAtMouse(BuildStatusTooltipLines(u8"방어도", entityData.block));
                                    }
                                    ++nextStatusLineY;
                                }

                                struct StatusToken {
                                    std::string label;
                                    int value = 0;
                                    WORD color = COLOR_WHITE;
                                };

                                std::vector<StatusToken> tokens;
                                if (entityData.strength > 0) {
                                    tokens.push_back({ u8"힘", entityData.strength, COLOR_RED });
                                }
                                if (entityData.dexterity > 0) {
                                    tokens.push_back({ u8"민첩", entityData.dexterity, FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY });
                                }
                                if (entityData.vulnerable > 0) {
                                    tokens.push_back({ u8"취약", entityData.vulnerable, COLOR_RED });
                                }
                                if (entityData.weak > 0) {
                                    tokens.push_back({ u8"약화", entityData.weak, COLOR_GREEN });
                                }

                                int tokenX = barX;
                                for (const StatusToken& token : tokens) {
                                    const std::string tokenText = token.label + std::to_string(token.value);
                                    const int tokenWidth = TextLayout::MeasureDisplayWidthUtf8(tokenText);
                                    const Rect tokenRect = { tokenX, nextStatusLineY, tokenWidth, 1 };
                                    screen.DrawString(tokenX, nextStatusLineY, tokenText, token.color);
                                    if (inputEnabled && tokenRect.Contains(mouseX, mouseY)) {
                                        showTooltipAtMouse(BuildStatusTooltipLines(token.label, token.value));
                                    }
                                    tokenX += tokenWidth + 2;
                                }
                                };

                            renderEntityStatusHud(playerEntityUi.get(), run.player);
                            renderEntityStatusHud(enemyEntityUi.get(), run.battleRoom.enemy);

                            if (powerBannerTimerSec > 0.0f && !powerBannerText.empty() && playerEntityUi) {
                                const int bannerX = TextLayout::ComputeAlignedXUtf8(
                                    playerEntityUi->GetHealthBarX(),
                                    playerEntityUi->GetHealthBarWidth(),
                                    powerBannerText,
                                    TextLayout::HorizontalAlign::Center);
                                const int bannerY = playerEntityUi->GetHealthBarY() + 4;
                                const WORD bannerColor = (powerBannerTimerSec > 0.35f) ? COLOR_YELLOW : FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
                                screen.DrawString(bannerX, bannerY, powerBannerText, bannerColor);
                            }

                            std::vector<int> renderOrder;
                            renderOrder.reserve(static_cast<size_t>(cardCount));
                            for (int cardIndex = 0; cardIndex < cardCount; ++cardIndex) {
                                renderOrder.push_back(cardIndex);
                            }
                            std::sort(renderOrder.begin(), renderOrder.end(), [&](int leftIndex, int rightIndex) {
                                return handCards[static_cast<size_t>(leftIndex)].GetX() < handCards[static_cast<size_t>(rightIndex)].GetX();
                                });

                            const bool hoveredCardCanFloat = allowCardInteraction &&
                                hoveredHandIndex >= 0 &&
                                hoveredHandIndex < cardCount &&
                                hand[static_cast<size_t>(hoveredHandIndex)].cost <= combatSystem->GetEnergy();

                            for (int cardIndex : renderOrder) {
                                if (cardIndex == draggedHandIndex || (draggedHandIndex < 0 && hoveredCardCanFloat && cardIndex == hoveredHandIndex)) {
                                    continue;
                                }
                                handCards[static_cast<size_t>(cardIndex)].Render(screen);
                            }
                            if (draggedHandIndex < 0 && hoveredCardCanFloat) {
                                handCards[static_cast<size_t>(hoveredHandIndex)].Render(screen);
                            }
                            if (draggedHandIndex >= 0 && draggedHandIndex < cardCount) {
                                handCards[static_cast<size_t>(draggedHandIndex)].Render(screen);
                            }
                            if (targetingArrow.IsActive()) {
                                targetingArrow.Render(screen);
                            }

                            if (escapeRequested) {
                                resolveBattleEscape();
                            }
                            else if (combatSystem->IsBattleOver()) {
                                if (combatSystem->DidPlayerWin()) {
                                    resolveBattleVictory();
                                }
                                else {
                                    PlayFixedEffect(audio, L"sfx\\STS_DeathStinger_v4_SFX.ogg", L"sfx_defeat", audioAliasCounter, settings.effectsEnabled);
                                    PlayRandomEffect(audio, deathMusicPool, L"bgm_defeat_stinger", audioRng, audioAliasCounter, settings.effectsEnabled);
                                    resolveBattleDefeat();
                                }
                            }
                        }
                        else {
                            resetCombatPresentation();
                            BattleRewardState& rewards = run.battleRoom.rewards;
                            if (!rewards.active) {
                                rewards.active = true;
                                rewards.title = u8"전리품";
                                rewards.message = !run.currentRoomSummaryText.empty() ? run.currentRoomSummaryText : u8"정리된 전투입니다.";
                            }

                            const Rect rewardRect = { roomPanel.x + roomPanel.width / 2 - 40, roomPanel.y + 10, 80, roomPanel.height - 18 };
                            if (renderRewardPanel(rewards, rewardRect, u8"보상 넘기기")) {
                                run.currentRoomSummaryTitle = rewards.title;
                                run.currentRoomSummaryText = rewards.message;
                                queueMapReturn();
                            }
                        }
                        break;
                    }
                    case RunNodeType::Shop:
                    {
                        const std::vector<std::string>& playerShopArt = AsciiArtLibrary::Get(AsciiArtId::PlayerBattle);
                        const std::vector<std::string>& merchantArt = AsciiArtLibrary::Get(AsciiArtId::Merchant);
                        const Rect playerClip = { 2, 8, 44, screen.GetHeight() - 10 };
                        const Rect merchantClip = { roomPanel.x + roomPanel.width / 2 + 6, 8, screen.GetWidth() - (roomPanel.x + roomPanel.width / 2 + 10), screen.GetHeight() - 10 };
                        const int playerCenterX = 19;
                        const int playerBottomY = screen.GetHeight() - 6;
                        const int merchantCenterX = roomPanel.x + roomPanel.width - 28;
                        const int merchantBottomY = screen.GetHeight() - 7;
                        const Rect merchantHitRect = { merchantCenterX - 22, merchantBottomY - 20, 42, 22 };
                        const bool merchantHovered = (!run.shopRoom.uiOpen && run.overlay == RunOverlayType::None && merchantHitRect.Contains(mouseX, mouseY));
                        RenderAnchoredArtClipped(screen, playerCenterX, playerBottomY, playerShopArt, COLOR_WHITE, playerClip);
                        RenderAnchoredArtClipped(screen, merchantCenterX, merchantBottomY, merchantArt, merchantHovered ? COLOR_YELLOW : COLOR_WHITE, merchantClip);

                        static const vector<string> kMerchantIdleLines = {
                            u8"뭘 살 텐가?",
                            u8"좋은 물건이 있지.",
                            u8"천천히 둘러봐.",
                            u8"흥정은 안 받는다."
                        };
                        static const vector<string> kMerchantOpenLines = {
                            u8"필요한 게 있나?",
                            u8"신중히 골라라.",
                            u8"오늘은 장사가 잘 되는군.",
                            u8"카드 제거도 가능하다."
                        };
                        const vector<string>& chatterLines = run.shopRoom.uiOpen ? kMerchantOpenLines : kMerchantIdleLines;
                        run.shopRoom.chatterTimerSec += deltaTimeSec;
                        if (!chatterLines.empty() && run.shopRoom.chatterTimerSec >= 4.5f) {
                            run.shopRoom.chatterTimerSec = 0.0f;
                            run.shopRoom.chatterIndex = (run.shopRoom.chatterIndex + 1) % static_cast<int>(chatterLines.size());
                            if (run.shopRoom.uiOpen) {
                                PlayRandomEffect(audio, merchantOpenVoicePool, L"vo_merchant_open_idle", audioRng, audioAliasCounter, settings.effectsEnabled);
                            }
                            else {
                                PlayRandomEffect(audio, merchantIdleVoicePool, L"vo_merchant_idle", audioRng, audioAliasCounter, settings.effectsEnabled);
                            }
                        }
                        const string chatterText = chatterLines.empty() ? u8"..." : chatterLines[static_cast<size_t>(run.shopRoom.chatterIndex % static_cast<int>(chatterLines.size()))];

                        if (!run.shopRoom.uiOpen) {
                            RenderWrappedText(screen, roomPanel.x + 4, 8, 40, run.shopRoom.noticeText, COLOR_YELLOW);
                            const Rect bubbleRect = { merchantCenterX - 44, merchantBottomY - 22, 26, 5 };
                            RenderSpeechBubble(screen, bubbleRect, chatterText, COLOR_WHITE, SpeechTailStyle::BottomRight);
                            ButtonUI btnLeave(roomPanel.x + 2, screen.GetHeight() - 6, 16, 3, u8"떠나기", COLOR_WHITE, COLOR_YELLOW);
                            if (run.overlay == RunOverlayType::None) {
                                btnLeave.Update(input);
                            }
                            UpdateButtonAudio(btnLeave, "shop_leave_scene", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                            btnLeave.Render(screen);

                            if (run.overlay == RunOverlayType::None && merchantHitRect.Contains(mouseX, mouseY) && input.IsLeftClickDown()) {
                                run.shopRoom.uiOpen = true;
                                run.shopRoom.noticeText = u8"원하는 물건을 눌러 구매하십시오.";
                                run.shopRoom.chatterTimerSec = 0.0f;
                            }
                            if (btnLeave.IsClicked()) {
                                run.currentRoomSummaryTitle = u8"상점";
                                run.currentRoomSummaryText = u8"상점 구역을 지나 다음 노드로 이동할 수 있습니다.";
                                ResolveCurrentNode(run, RunNodeResultType::Resolved);
                                queueMapReturn();
                            }
                        }
                        else {
                            const Rect shopRect = { 10, 8, screen.GetWidth() - 20, screen.GetHeight() - 14 };
                            RenderFrameBox(screen, shopRect, COLOR_WHITE);
                            RenderPanelTitle(screen, shopRect, u8"상점", COLOR_YELLOW);
                            RenderWrappedText(screen, shopRect.x + 3, shopRect.y + 2, shopRect.width - 6, run.shopRoom.noticeText, COLOR_YELLOW);

                            vector<ShopOfferState*> cardOffers;
                            vector<ShopOfferState*> relicOffers;
                            vector<ShopOfferState*> potionOffers;
                            for (ShopOfferState& offer : run.shopRoom.offers) {
                                switch (offer.type) {
                                case ShopOfferType::Card: cardOffers.push_back(&offer); break;
                                case ShopOfferType::Relic: relicOffers.push_back(&offer); break;
                                case ShopOfferType::Potion: potionOffers.push_back(&offer); break;
                                default: break;
                                }
                            }

                            const auto tryBuyOffer = [&](ShopOfferState& offer) {
                                if (offer.sold) {
                                    return;
                                }
                                if (run.gold < offer.price) {
                                    run.shopRoom.noticeText = u8"골드가 부족합니다.";
                                    PlayRandomEffect(audio, merchantNoGoldVoicePool, L"vo_merchant_no_gold", audioRng, audioAliasCounter, settings.effectsEnabled);
                                    return;
                                }
                                if (offer.type == ShopOfferType::Potion && !HasPotionSlot(run)) {
                                    run.shopRoom.noticeText = u8"포션 칸이 가득 찼습니다.";
                                    PlayRandomEffect(audio, merchantNoGoldVoicePool, L"vo_merchant_no_slot", audioRng, audioAliasCounter, settings.effectsEnabled);
                                    return;
                                }

                                run.gold -= offer.price;
                                offer.sold = true;
                                PlayFixedEffect(audio, L"sfx\\SOTE_SFX_CashRegister.ogg", L"sfx_shop_purchase", audioAliasCounter, settings.effectsEnabled);
                                PlayRandomEffect(audio, merchantBuyVoicePool, L"vo_merchant_buy", audioRng, audioAliasCounter, settings.effectsEnabled);
                                switch (offer.type) {
                                case ShopOfferType::Card:
                                    run.deck.push_back(offer.card);
                                    run.shopRoom.noticeText = offer.card.name + u8" 카드를 구매했습니다.";
                                    break;
                                case ShopOfferType::Relic:
                                    run.relics.push_back(offer.relic);
                                    run.shopRoom.noticeText = offer.relic.name + u8" 유물을 구매했습니다.";
                                    PlayRandomEffect(audio, relicDropPool, L"sfx_shop_relic", audioRng, audioAliasCounter, settings.effectsEnabled);
                                    break;
                                case ShopOfferType::Potion:
                                    run.potions.push_back(offer.potion);
                                    run.shopRoom.noticeText = offer.potion.name + u8" 포션을 구매했습니다.";
                                    PlayRandomEffect(audio, potionDropPool, L"sfx_shop_potion", audioRng, audioAliasCounter, settings.effectsEnabled);
                                    break;
                                default:
                                    break;
                                }
                            };

                            const auto renderOffer = [&](ShopOfferState& offer, const Rect& offerRect) {
                                const bool hovered = offerRect.Contains(mouseX, mouseY);
                                if (offer.type == ShopOfferType::Card) {
                                    RenderCompactCardOffer(screen, offerRect, offer.card, offer.price, offer.sold, hovered);
                                }
                                else {
                                    const WORD frameColor = offer.sold ? FOREGROUND_INTENSITY : (hovered ? COLOR_YELLOW : COLOR_WHITE);
                                    RenderFrameBox(screen, offerRect, frameColor);
                                    screen.DrawString(offerRect.x + 2, offerRect.y + 1, offer.title, frameColor);
                                    screen.DrawString(offerRect.x + 2, offerRect.y + 2, string(u8"가격 ") + to_string(offer.price) + "G", COLOR_YELLOW);
                                    RenderWrappedText(screen, offerRect.x + 2, offerRect.y + 3, offerRect.width - 4, offer.sold ? u8"구매 완료" : offer.description, COLOR_WHITE);
                                    if (offer.type == ShopOfferType::Relic && hovered) {
                                        showTooltipAtMouse(BuildRelicTooltipLines(offer.relic));
                                    }
                                }
                                if (!offer.sold && hovered && run.overlay == RunOverlayType::None && input.IsLeftClickDown()) {
                                    tryBuyOffer(offer);
                                }
                            };

                            const int topCardWidth = 22;
                            const int topCardHeight = 11;
                            const int topCardGap = 2;
                            const int topCardStartX = shopRect.x + 4;
                            const int topCardTop = shopRect.y + 5;
                            for (int index = 0; index < 5 && index < static_cast<int>(cardOffers.size()); ++index) {
                                const Rect offerRect = { topCardStartX + index * (topCardWidth + topCardGap), topCardTop, topCardWidth, topCardHeight };
                                renderOffer(*cardOffers[static_cast<size_t>(index)], offerRect);
                            }

                            const int lowerCardTop = shopRect.y + 18;
                            for (int index = 5; index < 7 && index < static_cast<int>(cardOffers.size()); ++index) {
                                const Rect offerRect = { shopRect.x + 6 + (index - 5) * 26, lowerCardTop, 24, 11 };
                                renderOffer(*cardOffers[static_cast<size_t>(index)], offerRect);
                            }

                            for (int index = 0; index < static_cast<int>(potionOffers.size()); ++index) {
                                const Rect offerRect = { shopRect.x + shopRect.width / 2 + 2 + index * 16, shopRect.y + shopRect.height - 11, 14, 5 };
                                renderOffer(*potionOffers[static_cast<size_t>(index)], offerRect);
                            }

                            for (int index = 0; index < static_cast<int>(relicOffers.size()); ++index) {
                                const Rect offerRect = { shopRect.x + shopRect.width / 2 + 2 + index * 16, shopRect.y + shopRect.height - 17, 14, 5 };
                                renderOffer(*relicOffers[static_cast<size_t>(index)], offerRect);
                            }

                            const Rect removalRect = { shopRect.x + shopRect.width - 24, shopRect.y + shopRect.height - 15, 20, 10 };
                            const bool removalHovered = removalRect.Contains(mouseX, mouseY);
                            RenderFrameBox(screen, removalRect, removalHovered ? COLOR_YELLOW : COLOR_WHITE);
                            RenderPanelTitle(screen, removalRect, u8"카드 제거", COLOR_YELLOW);
                            screen.DrawString(removalRect.x + 3, removalRect.y + 4, u8"서비스!", COLOR_WHITE);
                            screen.DrawString(removalRect.x + 4, removalRect.y + 6, to_string(run.shopRoom.removalPrice) + "G", COLOR_YELLOW);
                            screen.DrawString(removalRect.x + 2, removalRect.y + 7, run.shopRoom.removalUsed ? u8"사용 완료" : u8"클릭하여 진행", FOREGROUND_INTENSITY);
                            if (removalHovered && run.overlay == RunOverlayType::None && input.IsLeftClickDown()) {
                                if (run.shopRoom.removalUsed) {
                                    run.shopRoom.noticeText = u8"이번 상점에서는 이미 카드를 제거했습니다.";
                                }
                                else {
                                    run.shopRoom.removeMode = !run.shopRoom.removeMode;
                                    run.shopRoom.noticeText = run.shopRoom.removeMode ? u8"제거할 카드를 하나 선택하세요." : u8"카드 제거 모드를 종료했습니다.";
                                }
                            }

                            ButtonUI btnExit(shopRect.x + 3, shopRect.y + shopRect.height - 4, 16, 3, u8"돌아가기", COLOR_WHITE, COLOR_YELLOW);
                            if (run.overlay == RunOverlayType::None) {
                                btnExit.Update(input);
                            }
                            UpdateButtonAudio(btnExit, "shop_ui_back", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                            btnExit.Render(screen);
                            if (btnExit.IsClicked()) {
                                run.shopRoom.uiOpen = false;
                                run.shopRoom.removeMode = false;
                                run.shopRoom.noticeText = u8"상인을 눌러 물건을 살펴보십시오.";
                            }

                            if (run.shopRoom.removeMode) {
                                const Rect deckRect = { screen.GetCenterX() - 24, screen.GetCenterY() - 10, 48, 18 };
                                RenderFrameBox(screen, deckRect, COLOR_YELLOW);
                                RenderPanelTitle(screen, deckRect, u8"제거할 카드", COLOR_YELLOW);

                                for (size_t cardIndex = 0; cardIndex < run.deck.size() && cardIndex < static_cast<size_t>(deckRect.height - 4); ++cardIndex) {
                                    const Rect rowRect = { deckRect.x + 2, deckRect.y + 3 + static_cast<int>(cardIndex), deckRect.width - 4, 1 };
                                    const bool hovered = rowRect.Contains(mouseX, mouseY);
                                    screen.DrawString(rowRect.x, rowRect.y, TextLayout::AlignToWidth(TextLayout::Utf8ToWide(BuildCardSummary(run.deck[cardIndex])), rowRect.width, TextLayout::HorizontalAlign::Left), hovered ? COLOR_YELLOW : COLOR_WHITE);

                                    if (hovered && run.overlay == RunOverlayType::None && input.IsLeftClickDown()) {
                                        if (run.shopRoom.removalUsed) {
                                            run.shopRoom.noticeText = u8"이번 상점에서는 이미 카드를 제거했습니다.";
                                        }
                                        else if (run.gold < run.shopRoom.removalPrice) {
                                            run.shopRoom.noticeText = u8"카드 제거 비용이 부족합니다.";
                                            PlayRandomEffect(audio, merchantNoGoldVoicePool, L"vo_merchant_remove_no_gold", audioRng, audioAliasCounter, settings.effectsEnabled);
                                        }
                                        else if (run.deck.size() <= 1) {
                                            run.shopRoom.noticeText = u8"덱이 비어버리지 않도록 최소 1장은 남겨야 합니다.";
                                        }
                                        else {
                                            const string removedName = run.deck[cardIndex].name;
                                            run.gold -= run.shopRoom.removalPrice;
                                            run.deck.erase(run.deck.begin() + static_cast<vector<CardData>::difference_type>(cardIndex));
                                            run.shopRoom.removalUsed = true;
                                            run.shopRoom.removeMode = false;
                                            run.shopRoom.noticeText = removedName + u8" 카드를 제거했습니다.";
                                            PlayFixedEffect(audio, L"sfx\\SOTE_SFX_ExhaustCard.ogg", L"sfx_shop_remove_exhaust", audioAliasCounter, settings.effectsEnabled);
                                            PlayFixedEffect(audio, L"sfx\\SOTE_SFX_CashRegister.ogg", L"sfx_shop_remove_pay", audioAliasCounter, settings.effectsEnabled);
                                        }
                                        break;
                                    }
                                }
                            }

                            const Rect bubbleRect = { shopRect.x + shopRect.width - 38, 3, 30, 5 };
                            RenderSpeechBubble(screen, bubbleRect, chatterText, COLOR_WHITE, SpeechTailStyle::TopRight);
                        }
                        break;
                    }
                    case RunNodeType::Rest:
                    {
                        const std::vector<std::string>& playerRestArt = AsciiArtLibrary::Get(AsciiArtId::PlayerBattle);
                        const Rect playerClip = { 2, 8, 44, screen.GetHeight() - 10 };
                        const Rect fireClip = { screen.GetCenterX() - 8, 10, 60, screen.GetHeight() - 10 };
                        static float campfireAnimTimerSec = 0.0f;
                        campfireAnimTimerSec += deltaTimeSec;
                        const std::vector<std::vector<std::string>>& fireFrames = GetCampfireAnimFrames();
                        const int fireFrameIndex = static_cast<int>(campfireAnimTimerSec / 0.18f) % static_cast<int>(fireFrames.size());
                        RenderAnchoredArtClipped(screen, 18, screen.GetHeight() - 6, playerRestArt, COLOR_WHITE, playerClip);
                        RenderAnchoredArtClipped(screen, screen.GetCenterX() + 18, screen.GetHeight() - 6, fireFrames[static_cast<size_t>(fireFrameIndex)], COLOR_RED | COLOR_YELLOW, fireClip);

                        screen.DrawString(
                            TextLayout::ComputeAlignedXUtf8(0, screen.GetWidth(), u8"쉴 때가 됐군.", TextLayout::HorizontalAlign::Center),
                            8,
                            u8"쉴 때가 됐군.",
                            COLOR_WHITE);
                        RenderWrappedText(screen, screen.GetCenterX() - 22, 11, 44, run.restRoom.noticeText, FOREGROUND_INTENSITY);

                        if (run.restRoom.resultReady) {
                            RenderWrappedText(screen, screen.GetCenterX() - 20, 15, 40, run.restRoom.resultText, COLOR_WHITE);
                            ButtonUI btnContinue(screen.GetCenterX() - 10, 24, 20, 4, u8"계속", COLOR_WHITE, COLOR_YELLOW);
                            if (run.overlay == RunOverlayType::None) {
                                btnContinue.Update(input);
                            }
                            UpdateButtonAudio(btnContinue, "rest_continue", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                            btnContinue.Render(screen);
                            if (btnContinue.IsClicked()) {
                                run.currentRoomSummaryTitle = u8"휴식";
                                run.currentRoomSummaryText = run.restRoom.resultText;
                                ResolveCurrentNode(run, RunNodeResultType::Resolved);
                                queueMapReturn();
                            }
                        }
                        else {
                            ButtonUI btnRest(screen.GetCenterX() - 24, 14, 18, 4, u8"휴식", COLOR_WHITE, COLOR_YELLOW);
                            ButtonUI btnSmith(screen.GetCenterX() + 6, 14, 18, 4, u8"제련", COLOR_WHITE, COLOR_YELLOW);
                            if (run.overlay == RunOverlayType::None) {
                                btnRest.Update(input);
                                btnSmith.Update(input);
                            }
                            UpdateButtonAudio(btnRest, "rest_heal", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                            UpdateButtonAudio(btnSmith, "rest_smith", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                            btnRest.Render(screen);
                            btnSmith.Render(screen);

                            if (btnRest.IsClicked()) {
                                const int healAmount = (std::max)(12, run.player.maxHp / 4);
                                const int beforeHp = run.player.currentHp;
                                run.player.currentHp = (std::min)(run.player.maxHp, run.player.currentHp + healAmount);
                                run.restRoom.resultReady = true;
                                run.restRoom.resultText = string(u8"모닥불 곁에서 체력을 ") + to_string(run.player.currentHp - beforeHp) + u8" 회복했습니다.";
                                PlayRandomEffect(audio, sleepJinglePool, L"sfx_rest_heal", audioRng, audioAliasCounter, settings.effectsEnabled);
                            }
                            if (btnSmith.IsClicked()) {
                                run.restRoom.resultReady = true;
                                run.restRoom.resultText = u8"제련은 아직 구현되지 않았습니다.";
                                PlayFixedEffect(audio, L"sfx\\SOTE_SFX_UpgradeCard_v1.ogg", L"sfx_rest_smith", audioAliasCounter, settings.effectsEnabled);
                            }
                        }
                        break;
                    }
                    case RunNodeType::Treasure:
                    {
                        const std::vector<std::string>& playerTreasureArt = AsciiArtLibrary::Get(AsciiArtId::PlayerBattle);
                        const std::vector<std::string>& chestClosedArt = AsciiArtLibrary::Get(AsciiArtId::TreasureChestClosed);
                        const std::vector<std::string>& chestOpenArt = AsciiArtLibrary::Get(AsciiArtId::TreasureChestOpen);
                        const Rect playerClip = { 2, 8, 44, screen.GetHeight() - 10 };
                        const Rect chestClip = { roomPanel.x + roomPanel.width / 2 + 8, 10, screen.GetWidth() - (roomPanel.x + roomPanel.width / 2 + 12), screen.GetHeight() - 12 };
                        const int playerCenterX = 19;
                        const int playerBottomY = screen.GetHeight() - 6;
                        const int chestCenterX = roomPanel.x + roomPanel.width - 24;
                        const int chestBottomY = screen.GetHeight() - 7;
                        const Rect chestHitRect = { chestCenterX - 20, chestBottomY - 14, 34, 16 };
                        RenderAnchoredArtClipped(screen, playerCenterX, playerBottomY, playerTreasureArt, COLOR_WHITE, playerClip);
                        RenderAnchoredArtClipped(screen, chestCenterX, chestBottomY, run.treasureRoom.chestOpened ? chestOpenArt : chestClosedArt, COLOR_YELLOW, chestClip);

                        screen.DrawString(
                            TextLayout::ComputeAlignedXUtf8(0, screen.GetWidth(), u8"보물", TextLayout::HorizontalAlign::Center),
                            8,
                            u8"보물",
                            COLOR_YELLOW);
                        RenderWrappedText(screen, roomPanel.x + 4, 11, 42, run.treasureRoom.noticeText, COLOR_WHITE);

                        if (!run.treasureRoom.chestOpened) {
                            if (run.overlay == RunOverlayType::None && chestHitRect.Contains(mouseX, mouseY) && input.IsLeftClickDown()) {
                                run.treasureRoom.chestOpened = true;
                                run.treasureRoom.noticeText = u8"상자 안의 보상을 챙길 수 있습니다.";
                                PlayFixedEffect(audio, L"sfx\\SOTE_SFX_ChestOpen_v2.ogg", L"sfx_treasure_open", audioAliasCounter, settings.effectsEnabled);
                            }
                        }
                        else {
                            const Rect rewardRect = { roomPanel.x + roomPanel.width / 2 - 40, roomPanel.y + 10, 80, roomPanel.height - 18 };
                            if (renderRewardPanel(run.treasureRoom.rewards, rewardRect, u8"계속")) {
                                run.currentRoomSummaryTitle = u8"보물";
                                run.currentRoomSummaryText = u8"상자 안의 전리품을 챙기고 다음 노드로 이동할 수 있습니다.";
                                ResolveCurrentNode(run, RunNodeResultType::Resolved);
                                queueMapReturn();
                            }
                        }
                        break;
                    }
                    case RunNodeType::Event:
                    {
                        if (!run.eventRoom.noticeText.empty()) {
                            RenderWrappedText(screen, roomPanel.x + 28, roomPanel.y + 20, roomPanel.width - 34, run.eventRoom.noticeText, COLOR_YELLOW);
                        }

                        if (run.eventRoom.choiceCommitted) {
                            RenderWrappedText(screen, roomPanel.x + 28, roomPanel.y + 24, roomPanel.width - 34, run.eventRoom.resultTitle + string("\n") + run.eventRoom.resultText, COLOR_WHITE);
                            ButtonUI btnContinue(roomPanel.x + 28, roomPanel.y + roomPanel.height - 5, 18, 3, u8"계속", COLOR_WHITE, COLOR_YELLOW);
                            if (run.overlay == RunOverlayType::None) {
                                btnContinue.Update(input);
                            }
                            btnContinue.Render(screen);
                            if (btnContinue.IsClicked()) {
                                run.currentRoomSummaryTitle = run.eventRoom.resultTitle.empty() ? u8"미지 이벤트" : run.eventRoom.resultTitle;
                                run.currentRoomSummaryText = run.eventRoom.resultText;
                                ResolveCurrentNode(run, RunNodeResultType::Resolved);
                                queueMapReturn();
                            }
                        }
                        else {
                            const int buttonWidth = (std::max)(30, roomPanel.width / 3 - 8);
                            const int buttonHeight = 5;
                            const int buttonX = roomPanel.x + roomPanel.width - buttonWidth - 10;
                            const int buttonStartY = roomPanel.y + roomPanel.height - 7 - static_cast<int>(run.eventRoom.choices.size()) * (buttonHeight + 1);
                            for (size_t choiceIndex = 0; choiceIndex < run.eventRoom.choices.size(); ++choiceIndex) {
                                EventChoiceState& choice = run.eventRoom.choices[choiceIndex];
                                const int buttonY = buttonStartY + static_cast<int>(choiceIndex) * (buttonHeight + 1);
                                const Rect buttonRect = { buttonX, buttonY, buttonWidth, buttonHeight };
                                if (RenderWrappedActionButton(
                                    screen,
                                    input,
                                    buttonRect,
                                    mouseX,
                                    mouseY,
                                    { BuildEventChoiceButtonText(choice) },
                                    COLOR_WHITE,
                                    COLOR_YELLOW,
                                    run.overlay == RunOverlayType::None,
                                    TextLayout::HorizontalAlign::Center)) {
                                    if (choice.goldDelta < 0 && run.gold < -choice.goldDelta) {
                                        run.eventRoom.noticeText = u8"골드가 부족하여 이 선택을 진행할 수 없습니다.";
                                    }
                                    else if (choice.grantPotion && !HasPotionSlot(run)) {
                                        run.eventRoom.noticeText = u8"포션 칸이 가득 차서 이 선택을 진행할 수 없습니다.";
                                    }
                                    else {
                                        run.eventRoom.noticeText.clear();
                                        run.eventRoom.choiceCommitted = true;
                                        run.eventRoom.selectedChoiceId = choice.id;
                                        run.eventRoom.resultTitle = choice.resultTitle;
                                        run.eventRoom.resultText = choice.resultText;
                                        run.gold += choice.goldDelta;
                                        if (choice.hpDelta != 0) {
                                            if (choice.hpDelta > 0) {
                                                run.player.currentHp = (std::min)(run.player.maxHp, run.player.currentHp + choice.hpDelta);
                                            }
                                            else {
                                                run.player.currentHp = (std::max)(1, run.player.currentHp + choice.hpDelta);
                                            }
                                        }
                                        if (choice.grantRelic) {
                                            run.relics.push_back(choice.relic);
                                            run.eventRoom.resultText += "\n" + choice.relic.name + u8" 유물을 얻었습니다.";
                                        }
                                        if (choice.grantPotion) {
                                            run.potions.push_back(choice.potion);
                                            run.eventRoom.resultText += "\n" + choice.potion.name + u8" 포션을 얻었습니다.";
                                        }
                                        if (choice.grantCard) {
                                            run.deck.push_back(choice.card);
                                            run.eventRoom.resultText += "\n" + choice.card.name + u8" 카드를 덱에 추가했습니다.";
                                        }
                                        if (choice.goldDelta != 0) {
                                            PlayFixedEffect(audio, L"sfx\\SOTE_SFX_Gold_v1.ogg", L"sfx_event_gold", audioAliasCounter, settings.effectsEnabled);
                                        }
                                        if (choice.hpDelta > 0) {
                                            PlayRandomEffect(audio, healShortPool, L"sfx_event_heal", audioRng, audioAliasCounter, settings.effectsEnabled);
                                        }
                                        if (choice.grantRelic) {
                                            PlayRandomEffect(audio, relicDropPool, L"sfx_event_relic", audioRng, audioAliasCounter, settings.effectsEnabled);
                                        }
                                        if (choice.grantPotion) {
                                            PlayRandomEffect(audio, potionDropPool, L"sfx_event_potion", audioRng, audioAliasCounter, settings.effectsEnabled);
                                        }
                                    }
                                }
                            }
                        }
                        break;
                    }
                    }
                }
            }

            if (run.overlay != RunOverlayType::Map) {
                mapRenderer.ClearViewport();
            }

            if (run.overlay == RunOverlayType::Map) {
                const Rect overlayRect = { 6, 5, screen.GetWidth() - 12, screen.GetHeight() - 10 };
                const Rect mapViewport = { overlayRect.x + 2, overlayRect.y + 3, overlayRect.width - 4, overlayRect.height - 7 };
                RenderFrameBox(screen, overlayRect, COLOR_YELLOW);
                RenderPanelTitle(screen, overlayRect, u8"지도", COLOR_YELLOW);
                screen.DrawString(overlayRect.x + 3, overlayRect.y + 2, u8"M 또는 ESC로 닫기 / 휠로 스크롤 / 클릭으로 진입", FOREGROUND_INTENSITY);

                ButtonUI btnReturnRoom(overlayRect.x + overlayRect.width - 20, overlayRect.y + overlayRect.height - 4, 16, 3, u8"돌아가기", COLOR_WHITE, COLOR_YELLOW);
                if (run.currentNodeId >= 0) {
                    btnReturnRoom.Update(input);
                    UpdateButtonAudio(btnReturnRoom, "map_return_room", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                    btnReturnRoom.Render(screen);
                    if (btnReturnRoom.IsClicked()) {
                        run.overlay = RunOverlayType::None;
                        tooltip.SetVisible(false);
                    }
                }

                mapRenderer.SetViewport(mapViewport.x, mapViewport.y, mapViewport.width, mapViewport.height);
                mapRenderer.Update(input);
                mapRenderer.Render(screen);

                int hoveredNodeId = -1;
                vector<string> tooltipLines;
                if (mapRenderer.TryGetHoveredNodeId(mouseX, mouseY, hoveredNodeId) && mapRenderer.TryGetNodeTooltip(mouseX, mouseY, tooltipLines)) {
                    if (hoveredNodeId >= 0 && hoveredNodeId != lastHoveredMapNodeId) {
                        PlayRandomEffect(audio, mapHoverPool, L"sfx_map_hover", audioRng, audioAliasCounter, settings.effectsEnabled);
                    }
                    lastHoveredMapNodeId = hoveredNodeId;
                    showTooltipAtMouse(tooltipLines);

                    const bool canSelectNode = (run.currentNodeId < 0 || run.roomResolved) && CanEnterNode(run, hoveredNodeId);
                    if (canSelectNode && input.IsLeftClickDown()) {
                        const RunNodeState* selectedNode = FindNodeById(run, hoveredNodeId);
                        PlayRandomEffect(audio, mapSelectPool, L"sfx_map_select", audioRng, audioAliasCounter, settings.effectsEnabled);
                        if (selectedNode != nullptr && IsBattleNodeType(selectedNode->type)) {
                            PlayRandomEffect(audio, battleStartPool, L"sfx_map_battle_start", audioRng, audioAliasCounter, settings.effectsEnabled);
                        }
                        EnterNode(run, hoveredNodeId);
                        SaveManager::SaveContinueRun(run);
                        tooltip.SetVisible(false);
                        run.overlay = RunOverlayType::None;
                    }
                }
                else {
                    lastHoveredMapNodeId = -1;
                    tooltip.SetVisible(false);
                }
            }
            else if (run.overlay == RunOverlayType::Deck) {
                const Rect overlayRect = { 10, 5, screen.GetWidth() - 20, screen.GetHeight() - 10 };
                RenderFrameBox(screen, overlayRect, COLOR_YELLOW);
                RenderPanelTitle(screen, overlayRect, u8"현재 덱", COLOR_YELLOW);

                vector<string> deckLines = BuildDeckLines(run);
                if (input.GetWheelDelta() != 0) {
                    deckScroll -= input.GetWheelDelta();
                }

                const int visibleLineCount = overlayRect.height - 6;
                deckScroll = (std::max)(0, (std::min)(deckScroll, (std::max)(0, static_cast<int>(deckLines.size()) - visibleLineCount)));

                for (int lineIndex = 0; lineIndex < visibleLineCount && (deckScroll + lineIndex) < static_cast<int>(deckLines.size()); ++lineIndex) {
                    screen.DrawString(overlayRect.x + 3, overlayRect.y + 3 + lineIndex, deckLines[static_cast<size_t>(deckScroll + lineIndex)], COLOR_WHITE);
                }
            }
            else if (run.overlay == RunOverlayType::Settings) {
                const Rect popup = { screen.GetCenterX() - 46, screen.GetCenterY() - 12, 92, 25 };
                RenderFrameBox(screen, popup, COLOR_YELLOW);
                RenderPanelTitle(screen, popup, u8"인게임 설정", COLOR_YELLOW);

                const int contentLeft = popup.x + 4;
                const int contentWidth = popup.width - 8;
                Rect masterTrack = { contentLeft + 12, popup.y + 5, kRunSettingsSliderWidth, 1 };
                Rect bgmTrack = { contentLeft + 12, popup.y + 7, kRunSettingsSliderWidth, 1 };
                Rect sfxTrack = { contentLeft + 12, popup.y + 9, kRunSettingsSliderWidth, 1 };
                Rect speedTrack = { contentLeft + 12, popup.y + 11, kRunSettingsSliderWidth, 1 };

                UpdateSliderDrag(input, mouseX, 0, masterTrack, settings.masterVolume, activeSliderId);
                UpdateSliderDrag(input, mouseX, 1, bgmTrack, settings.bgmVolume, activeSliderId);
                UpdateSliderDrag(input, mouseX, 2, sfxTrack, settings.sfxVolume, activeSliderId);
                UpdateSliderDrag(input, mouseX, 3, speedTrack, settings.gameSpeedPercent, activeSliderId);

                RenderSlider(screen, contentLeft, popup.y + 5, kRunSettingsSliderWidth, u8"마스터", settings.masterVolume, activeSliderId == 0);
                RenderSlider(screen, contentLeft, popup.y + 7, kRunSettingsSliderWidth, u8"BGM", settings.bgmVolume, activeSliderId == 1);
                RenderSlider(screen, contentLeft, popup.y + 9, kRunSettingsSliderWidth, u8"SFX", settings.sfxVolume, activeSliderId == 2);
                RenderSlider(screen, contentLeft, popup.y + 11, kRunSettingsSliderWidth, u8"속도", settings.gameSpeedPercent, activeSliderId == 3);

                ButtonUI btnToggleDebug(contentLeft, popup.y + 15, 18, 3, string(u8"디버그 ") + (settings.debugMode ? u8"ON" : u8"OFF"), COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnBack(contentLeft, popup.y + popup.height - 4, 16, 3, u8"돌아가기", COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnAbandon(contentLeft + 18, popup.y + popup.height - 4, 18, 3, u8"전투 포기", COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnSaveExit(contentLeft + 38, popup.y + popup.height - 4, 20, 3, u8"저장 후 종료", COLOR_WHITE, COLOR_YELLOW);
                btnToggleDebug.Update(input);
                btnBack.Update(input);
                btnAbandon.Update(input);
                btnSaveExit.Update(input);
                UpdateButtonAudio(btnToggleDebug, "run_settings_debug", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                UpdateButtonAudio(btnBack, "run_settings_back", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                UpdateButtonAudio(btnAbandon, "run_settings_abandon", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                UpdateButtonAudio(btnSaveExit, "run_settings_save_exit", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                btnToggleDebug.Render(screen);
                btnBack.Render(screen);
                btnAbandon.Render(screen);
                btnSaveExit.Render(screen);
                screen.DrawString(contentLeft + 22, popup.y + 15, u8"키 설정: 틀만 준비", FOREGROUND_INTENSITY);

                if (btnToggleDebug.IsClicked()) { settings.debugMode = !settings.debugMode; settingsDirty = true; }
                if (btnBack.IsClicked()) {
                    SaveManager::SaveSettings(settings);
                    settingsDirty = false;
                    run.overlay = RunOverlayType::None;
                    activeSliderId = -1;
                }
                if (btnAbandon.IsClicked()) {
                    run.pendingConfirm = ConfirmActionType::AbandonFromRun;
                    run.overlay = RunOverlayType::Confirm;
                }
                if (btnSaveExit.IsClicked()) {
                    run.pendingConfirm = ConfirmActionType::SaveAndExit;
                    run.overlay = RunOverlayType::Confirm;
                }
                else {
                    settingsDirty = true;
                }
            }
            else if (run.overlay == RunOverlayType::Confirm) {
                const Rect popup = { screen.GetCenterX() - 24, screen.GetCenterY() - 5, 48, 11 };
                RenderFrameBox(screen, popup, COLOR_RED);
                const bool abandonConfirm = (run.pendingConfirm == ConfirmActionType::AbandonFromRun);
                RenderPanelTitle(screen, popup, abandonConfirm ? u8"전투 포기" : u8"저장 후 종료", COLOR_RED);
                RenderWrappedText(
                    screen,
                    popup.x + 3,
                    popup.y + 4,
                    popup.width - 6,
                    abandonConfirm
                    ? u8"포기 시 패배로 처리됩니다. 정말 현재 런을 포기하시겠습니까?"
                    : u8"현재 진행 상태를 저장하고 타이틀로 돌아가시겠습니까?",
                    COLOR_WHITE);

                ButtonUI btnYes(popup.x + 7, popup.y + popup.height - 4, 12, 3, u8"네", COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnNo(popup.x + popup.width - 19, popup.y + popup.height - 4, 12, 3, u8"아니오", COLOR_WHITE, COLOR_YELLOW);
                btnYes.Update(input);
                btnNo.Update(input);
                UpdateButtonAudio(btnYes, "run_confirm_yes", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                UpdateButtonAudio(btnNo, "run_confirm_no", audio, uiHoverPool, uiClickPool, hoverAudioLatch, audioRng, audioAliasCounter, settings.effectsEnabled);
                btnYes.Render(screen);
                btnNo.Render(screen);

                if (btnYes.IsClicked()) {
                    if (run.pendingConfirm == ConfirmActionType::AbandonFromRun) {
                        abandonRunFromSettings();
                    }
                    else if (run.pendingConfirm == ConfirmActionType::SaveAndExit) {
                        saveAndExitRun();
                    }
                    run.pendingConfirm = ConfirmActionType::None;
                }
                if (btnNo.IsClicked() || pressedEsc) {
                    run.pendingConfirm = ConfirmActionType::None;
                    run.overlay = RunOverlayType::Settings;
                }
            }
            else if (run.overlay == RunOverlayType::Ending && !IsBattleNodeType(run.currentRoomType)) {
                if (endingStage == EndingStage::DefeatReveal) {
                    const Rect panel = { screen.GetCenterX() - 28, screen.GetCenterY() - 5, 56, 6 };
                    RenderFrameBox(screen, panel, COLOR_RED);
                    screen.DrawString(
                        panel.x + 2,
                        panel.y + 2,
                        TextLayout::AlignToWidth(TextLayout::Utf8ToWide(endingFlavorText), panel.width - 4, TextLayout::HorizontalAlign::Center),
                        COLOR_RED);
                    ButtonUI btnContinue(screen.GetCenterX() - 8, screen.GetCenterY() + 4, 16, 3, u8"계속", COLOR_WHITE, COLOR_YELLOW);
                    btnContinue.Update(input);
                    btnContinue.Render(screen);
                    if (btnContinue.IsClicked()) {
                        endingStage = EndingStage::DefeatRecord;
                    }
                }
                else if (endingStage == EndingStage::DefeatRecord) {
                    const Rect recordRect = { screen.GetCenterX() - 34, screen.GetCenterY() - 8, 68, 16 };
                    RenderFrameBox(screen, recordRect, COLOR_RED);
                    RenderPanelTitle(screen, recordRect, u8"패배 기록", COLOR_RED);
                    RenderTextBlock(
                        screen,
                        { recordRect.x + 3, recordRect.y + 3, recordRect.width - 6, recordRect.height - 7 },
                        BuildRunRecordDetailLines(endingRecord),
                        COLOR_WHITE);
                    ButtonUI btnMenu(screen.GetCenterX() - 9, screen.GetCenterY() + 9, 18, 3, u8"메인 메뉴", COLOR_WHITE, COLOR_YELLOW);
                    btnMenu.Update(input);
                    btnMenu.Render(screen);
                    if (btnMenu.IsClicked()) {
                        transitionToTitle();
                    }
                }
                else {
                    const Rect panel = { screen.GetCenterX() - 24, screen.GetCenterY() - 5, 48, 8 };
                    RenderFrameBox(screen, panel, COLOR_GREEN);
                    RenderPanelTitle(screen, panel, u8"승리!", COLOR_GREEN);
                    RenderWrappedText(screen, panel.x + 3, panel.y + 3, panel.width - 6, u8"막의 끝에 도달했습니다.", COLOR_WHITE);
                    ButtonUI btnContinue(screen.GetCenterX() - 8, screen.GetCenterY() + 5, 16, 3, u8"진행", COLOR_WHITE, COLOR_YELLOW);
                    btnContinue.Update(input);
                    btnContinue.Render(screen);
                    if (btnContinue.IsClicked()) {
                        transitionToTitle();
                    }
                }
            }

            if (!tooltipClaimedThisFrame && run.overlay != RunOverlayType::Map) {
                tooltip.SetVisible(false);
            }

            if (tooltip.IsVisible()) {
                tooltip.Render(screen);
            }

            const int debugBaseY = 5;
            const string fpsText = "FPS: " + to_string(static_cast<int>(round(displayedFps)));
            const string frameText = "Frame: " + to_string(static_cast<int>(round(displayedFrameMs))) + " ms";
            const string seedText = "Seed: " + to_string(run.seed);
            screen.DrawString(2, debugBaseY, fpsText, COLOR_GREEN);
            screen.DrawString(16, debugBaseY, frameText, COLOR_GREEN);
            screen.DrawString(36, debugBaseY, seedText, FOREGROUND_INTENSITY);
        }
        else if (appState == AppState::Ending) {
            transitionToTitle();
        }

        if (artPreviewOpen) {
            const std::vector<ArtPreviewEntry>& previewEntries = GetArtPreviewEntries();
            if (!previewEntries.empty()) {
                const ArtPreviewEntry& preview = previewEntries[static_cast<size_t>(artPreviewIndex % static_cast<int>(previewEntries.size()))];
                const Rect previewRect = { 6, 3, screen.GetWidth() - 12, screen.GetHeight() - 6 };
                const Rect artClip = { previewRect.x + 2, previewRect.y + 3, previewRect.width - 4, previewRect.height - 7 };
                const std::vector<std::string>& artLines = AsciiArtLibrary::Get(preview.id);

                RenderFrameBox(screen, previewRect, COLOR_BLUE);
                RenderPanelTitle(screen, previewRect, u8"아트 프리뷰", COLOR_BLUE);
                screen.DrawString(
                    previewRect.x + 2,
                    previewRect.y + 1,
                    preview.name + "  " + std::to_string(artPreviewIndex + 1) + "/" + std::to_string(previewEntries.size()),
                    preview.color);
                screen.DrawString(
                    previewRect.x + 2,
                    previewRect.y + previewRect.height - 2,
                    u8"Ctrl+F11 닫기  Ctrl+F12/←/→ 전환",
                    FOREGROUND_INTENSITY);
                RenderCenteredArtClipped(
                    screen,
                    artClip.x + (artClip.width / 2),
                    artClip.y + (artClip.height / 2),
                    artLines,
                    preview.color,
                    artClip);
            }
        }

        screen.DrawChar(mouseX, mouseY, '+', COLOR_GREEN);
        screen.Render();

        const auto frameEndTime = Clock::now();
        const auto frameWorkTime = frameEndTime - frameStartTime;
        if (frameWorkTime < targetFrameTime) {
            const auto remainingTime = targetFrameTime - frameWorkTime;
            const auto sleepTime = chrono::duration_cast<chrono::milliseconds>(remainingTime);

            if (sleepTime.count() > 1) {
                Sleep(static_cast<DWORD>(sleepTime.count() - 1));
            }

            while ((Clock::now() - frameStartTime) < targetFrameTime) {
                Sleep(0);
            }
        }
    }

    if (settingsDirty) {
        SaveManager::SaveSettings(settings);
    }

    return 0;
}
