// -----------------------------------------------------------------------------
// @file       main.cpp
// @brief      Application entry point for the title/run state machine skeleton.
// -----------------------------------------------------------------------------
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "AudioManager.h"
#include "ButtonUI.h"
#include "CardUI.h"
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

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool Contains(int px, int py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }
};

enum class EndingStage {
    None,
    DefeatReveal,
    DefeatRecord,
    VictoryReveal,
    VictorySummary
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

std::vector<std::string> BuildDeathPlayerArt() {
    return {
        u8"⢀⣀⣀⣀⣀",
        u8"⣠⣴⣶⣶⣶⣶⣶⣶⣾⣿⣿⣿⣿⣿⣿⣟",
        u8"⢀⣴⣤⣤⣤⣴⣶⣦⣶⣶⣿⣿⣿⣿⣿⣿⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡏",
        u8"⠿⠟⠋⠛⣻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡟⠛⠩⠛⠻⠿⠿⣿⡿⢟⣟⢠⣄⡀",
        u8"⢀⣾⣿⣿⣿⣿⣿⣿⣿⡿⠿⠛⠛⠉⠉⠀⠀⠀⠀⠀⠀⠀⢠⣀⣠⣀⣴⣿⠿⠿⢷⣿⣶",
        u8"⡾⠋⠁⠀⠀⠀⠀⣀⣀⣠⣤⣤⣤⣴⣶⡶⠶⠾⠿⠟⠛⠛⠋⠉⠏",
        u8"⠠⢘⠏⠉⠁"
    };
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
    const string gaugeInner(static_cast<size_t>(width - 8), '.');
    string gauge = gaugeInner;
    const int fillCount = static_cast<int>(std::round((static_cast<float>(gauge.size()) * value) / 100.0f));
    for (int index = 0; index < fillCount && index < static_cast<int>(gauge.size()); ++index) {
        gauge[static_cast<size_t>(index)] = '#';
    }

    const WORD color = active ? COLOR_YELLOW : COLOR_WHITE;
    screen.DrawString(x, y, label, color);
    screen.DrawString(x + 14, y, "[" + gauge + "]", color);
    screen.DrawString(x + width - 4, y, to_string(value), color);
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

vector<CardData> BuildBattleRewardCardPool() {
    return {
        MakeRewardCard(8000, u8"강타+", 1, u8"적에게 8 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardEffectType::AttackDamage, CardDiscardEffectType::None, 8, 0),
        MakeRewardCard(8001, u8"수비+", 1, u8"방어도 7을 얻습니다.", CardType::Skill, CardTargetType::Self, CardEffectType::DefendBlock, CardDiscardEffectType::None, 7, 0),
        MakeRewardCard(8002, u8"재정비", 0, u8"버릴 때 카드를 1장 뽑고 에너지 1을 얻습니다.", CardType::Skill, CardTargetType::None, CardEffectType::None, CardDiscardEffectType::DrawCardsGainEnergy, 1, 1),
        MakeRewardCard(8003, u8"약점 노출", 2, u8"적에게 취약 2를 부여합니다.", CardType::Skill, CardTargetType::Enemy, CardEffectType::ApplyVulnerable, CardDiscardEffectType::None, 2, 0),
        MakeRewardCard(8004, u8"대검", 2, u8"적에게 12 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardEffectType::AttackDamage, CardDiscardEffectType::None, 12, 0),
        MakeRewardCard(8005, u8"굳건한 자세", 2, u8"방어도 10을 얻습니다.", CardType::Skill, CardTargetType::Self, CardEffectType::DefendBlock, CardDiscardEffectType::None, 10, 0),
        MakeRewardCard(8006, u8"폼멜 타격", 1, u8"적에게 9 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardEffectType::AttackDamage, CardDiscardEffectType::None, 9, 0),
        MakeRewardCard(8007, u8"압박", 1, u8"적에게 7 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardEffectType::AttackDamage, CardDiscardEffectType::None, 7, 0)
    };
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
        vector<int> indices;
        indices.reserve(cardPool.size());
        for (int index = 0; index < static_cast<int>(cardPool.size()); ++index) {
            indices.push_back(index);
        }
        shuffle(indices.begin(), indices.end(), rng);
        for (int pickIndex = 0; pickIndex < 3 && pickIndex < static_cast<int>(indices.size()); ++pickIndex) {
            reward.cardChoices.push_back(cardPool[static_cast<size_t>(indices[static_cast<size_t>(pickIndex)])]);
        }
    }

    return reward;
}

bool HasBattleRewardItemsRemaining(const BattleRewardState& reward) {
    return (reward.goldAvailable && !reward.goldClaimed) ||
        (reward.potionAvailable && !reward.potionClaimed) ||
        (reward.cardRewardAvailable && !reward.cardRewardClaimed);
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

} // namespace

int main() {
    SetConsoleOutputCP(CP_UTF8);

    ScreenManager screen;
    InputManager input;
    AudioManager audio;
    TooltipUI tooltip;
    MapRenderer mapRenderer(screen.GetWidth(), screen.GetHeight());

    SettingsData settings = SaveManager::LoadSettings();
    GlobalStatsData globalStats = SaveManager::LoadGlobalStats();
    vector<RunRecordData> runRecords = SaveManager::LoadRunRecords();
    vector<CardPackOption> starterPacks = BuildStarterCardPacks();

    audio.SetVolumes(settings.masterVolume, settings.bgmVolume, settings.sfxVolume);
    audio.PlayBGM(L"Slay the Spire.wav", 100.0f);

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
        targetingArrow.SetActive(false);
        };

    auto queueRoomBgm = [&]() {
        std::wstring track = L"Slay the Spire.wav";
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
        audio.QueueBGMFade(L"Slay the Spire.wav", 100.0f, 0.2f, 0.45f);
        lastQueuedBgmTrack = L"Slay the Spire.wav";
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
            if (pressedF5 && combatSystem && !run.roomResolved && IsBattleNodeType(run.currentRoomType)) {
                run.battleRoom.enemy.currentHp = 0;
            }
            if (pressedF6 && combatSystem && !run.roomResolved && IsBattleNodeType(run.currentRoomType)) {
                run.player.currentHp = 0;
            }
            if (pressedF7) openDebugRoom(RunNodeType::Shop);
            if (pressedF8) openDebugRoom(RunNodeType::Rest);
            if (pressedF9) openDebugRoom(RunNodeType::Treasure);
            if (pressedF10) openDebugRoom(RunNodeType::Battle);
            if (pressedF11) openDebugRoom(RunNodeType::Elite);
            if (pressedF12) openDebugRoom(RunNodeType::Boss);
        }

        if (appState == AppState::Run && run.overlay != RunOverlayType::Ending) {
            queueRoomBgm();
        }

        screen.Clear();

        if (appState == AppState::Title) {
            if (pressedEsc && titleOverlay == TitleOverlayType::None) {
                shouldQuit = true;
            }

            const Rect logoRect = { screen.GetCenterX() - 24, 5, 48, 7 };
            RenderFrameBox(screen, logoRect, COLOR_WHITE);
            RenderPanelTitle(screen, logoRect, u8"SLAY THE SPIRE", COLOR_RED);
            screen.DrawString(
                logoRect.x + 2,
                logoRect.y + 3,
                TextLayout::AlignToWidth(TextLayout::Utf8ToWide(u8"상태 머신 / 런 뼈대 단계"), logoRect.width - 4, TextLayout::HorizontalAlign::Center),
                COLOR_WHITE);
            screen.DrawString(
                logoRect.x + 2,
                logoRect.y + 4,
                TextLayout::AlignToWidth(TextLayout::Utf8ToWide(string(u8"시드 준비 / 기록 ") + to_string(runRecords.size())), logoRect.width - 4, TextLayout::HorizontalAlign::Center),
                FOREGROUND_INTENSITY);

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
            for (auto& button : titleButtons) {
                if (titleButtonsInteractive) {
                    button.Update(input);
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
                Rect masterTrack = { contentLeft + 12, popup.y + 5, contentWidth - 20, 1 };
                Rect bgmTrack = { contentLeft + 12, popup.y + 7, contentWidth - 20, 1 };
                Rect sfxTrack = { contentLeft + 12, popup.y + 9, contentWidth - 20, 1 };
                Rect speedTrack = { contentLeft + 12, popup.y + 11, contentWidth - 20, 1 };

                UpdateSliderDrag(input, mouseX, 0, masterTrack, settings.masterVolume, activeSliderId);
                UpdateSliderDrag(input, mouseX, 1, bgmTrack, settings.bgmVolume, activeSliderId);
                UpdateSliderDrag(input, mouseX, 2, sfxTrack, settings.sfxVolume, activeSliderId);
                UpdateSliderDrag(input, mouseX, 3, speedTrack, settings.gameSpeedPercent, activeSliderId);

                RenderSlider(screen, contentLeft, popup.y + 5, contentWidth, u8"마스터", settings.masterVolume, activeSliderId == 0);
                RenderSlider(screen, contentLeft, popup.y + 7, contentWidth, u8"BGM", settings.bgmVolume, activeSliderId == 1);
                RenderSlider(screen, contentLeft, popup.y + 9, contentWidth, u8"SFX", settings.sfxVolume, activeSliderId == 2);
                RenderSlider(screen, contentLeft, popup.y + 11, contentWidth, u8"속도", settings.gameSpeedPercent, activeSliderId == 3);

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

                ButtonUI btnBack(popup.x + popup.width - 18, popup.y + popup.height - 4, 14, 3, u8"돌아가기", COLOR_WHITE, COLOR_YELLOW);
                btnBack.Update(input);
                btnBack.Render(screen);

                if (pressedEsc || btnBack.IsClicked()) {
                    titleOverlay = TitleOverlayType::None;
                }

                if (!runRecords.empty()) {
                    const int listTop = popup.y + 5;
                    const int visibleCount = popup.height - 12;
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

                    const Rect detailRect = { popup.x + popup.width / 2, popup.y + 4, popup.width / 2 - 3, popup.height - 6 };
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
            const bool allowHudOverlayToggle = (run.overlay != RunOverlayType::Confirm && !endingOverlayOpen);

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
                }
                btnSettings.Update(input);

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
                const Rect headline = { screen.GetCenterX() - 32, 7, 64, 7 };
                const Rect collapsedDetailRect = { screen.GetCenterX() - 26, 39, 52, 7 };
                const Rect expandedDetailRect = { 2, 34, screen.GetWidth() - 4, (screen.GetHeight() - 9) - 34 };
                RenderFrameBox(screen, headline, COLOR_YELLOW);
                screen.DrawString(
                    headline.x + 1,
                    headline.y + 1,
                    TextLayout::AlignToWidth(TextLayout::Utf8ToWide(u8"시너지 카드팩 선택"), headline.width - 2, TextLayout::HorizontalAlign::Center),
                    COLOR_YELLOW);
                RenderWrappedText(screen, headline.x + 3, headline.y + 3, headline.width - 6, u8"첫 시작에서는 반드시 3개 중 하나의 카드팩을 고른 뒤 확인 버튼으로 런을 시작합니다.", COLOR_WHITE);

                std::vector<Rect> packRects;
                packRects.reserve(starterPacks.size());
                int hoveredPackIndex = -1;
                for (size_t packIndex = 0; packIndex < starterPacks.size(); ++packIndex) {
                    const int panelWidth = 28;
                    const int panelHeight = 20;
                    const int startX = screen.GetCenterX() - 46 + static_cast<int>(packIndex) * 31;
                    const Rect packRect = { startX, 15, panelWidth, panelHeight };
                    packRects.push_back(packRect);
                    const bool hovered = packRect.Contains(mouseX, mouseY);
                    const bool selected = (run.selectedStarterPackIndex == static_cast<int>(packIndex));
                    const WORD frameColor = selected || hovered ? starterPacks[packIndex].accentColor : COLOR_WHITE;
                    if (hovered) {
                        hoveredPackIndex = static_cast<int>(packIndex);
                    }

                    RenderFrameBox(screen, packRect, frameColor);
                    RenderPanelTitle(screen, packRect, starterPacks[packIndex].title, frameColor);
                    RenderWrappedText(screen, packRect.x + 2, packRect.y + 4, packRect.width - 4, starterPacks[packIndex].description, COLOR_WHITE);

                    int drawY = packRect.y + 8;
                    for (const CardData& card : starterPacks[packIndex].cards) {
                        if (drawY >= packRect.y + panelHeight - 4) {
                            break;
                        }
                        screen.DrawString(packRect.x + 2, drawY, BuildCardSummary(card), COLOR_WHITE);
                        drawY += 2;
                    }

                    screen.DrawString(packRect.x + 2, packRect.y + panelHeight - 3, selected ? u8"선택됨" : u8"클릭하여 선택", selected ? frameColor : FOREGROUND_INTENSITY);
                    if (hovered && input.IsLeftClickDown()) {
                        run.selectedStarterPackIndex = static_cast<int>(packIndex);
                        animatedPackIndex = static_cast<int>(packIndex);
                        cardPackPanelProgress = (std::max)(cardPackPanelProgress, 0.35f);
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
                const Rect animatedDetailRect = LerpRectFixedTopCenter(collapsedDetailRect, expandedDetailRect, easedPanelProgress);
                const WORD detailColor =
                    (previewIndex >= 0 && previewIndex < static_cast<int>(starterPacks.size()))
                    ? starterPacks[static_cast<size_t>(previewIndex)].accentColor
                    : COLOR_WHITE;

                RenderFrameBox(screen, animatedDetailRect, detailColor);

                if (previewIndex >= 0 && previewIndex < static_cast<int>(starterPacks.size())) {
                    const CardPackOption& previewPack = starterPacks[static_cast<size_t>(previewIndex)];
                    const int stableTitleX = TextLayout::ComputeAlignedXUtf8(
                        expandedDetailRect.x + 1,
                        expandedDetailRect.width - 2,
                        previewPack.title,
                        TextLayout::HorizontalAlign::Center);
                    screen.DrawString(stableTitleX, animatedDetailRect.y + 1, previewPack.title, detailColor);

                    const Rect packSummaryRect = { expandedDetailRect.x + expandedDetailRect.width / 2, expandedDetailRect.y + 4, expandedDetailRect.width / 2 - 4, 6 };
                    const Rect cardDetailRect = { expandedDetailRect.x + expandedDetailRect.width / 2, expandedDetailRect.y + 12, expandedDetailRect.width / 2 - 4, 9 };

                    std::vector<Rect> cardPreviewRects;
                    cardPreviewRects.reserve(previewPack.cards.size());
                    const int previewCardWidth = 16;
                    const int previewCardHeight = 7;
                    const int previewCardGapX = 3;
                    const int previewCardGapY = 2;
                    const int cardGridLeft = expandedDetailRect.x + 3;
                    const int cardGridTop = expandedDetailRect.y + 4;

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
                        const std::vector<std::string> summaryLines = {
                            previewPack.description,
                            string(u8"포함 카드 수: ") + to_string(previewPack.cards.size()),
                            string(u8"추천 성향: ") + (previewIndex == 0 ? u8"공격 템포" : (previewIndex == 1 ? u8"안정적인 운영" : u8"유틸과 변칙"))
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

                ButtonUI btnConfirm(screen.GetCenterX() - 12, screen.GetHeight() - 6, 24, 3, u8"이 카드팩으로 시작", COLOR_WHITE, COLOR_YELLOW);
                if (run.selectedStarterPackIndex >= 0) {
                    btnConfirm.Update(input);
                }
                btnConfirm.Render(screen);
                if (run.selectedStarterPackIndex < 0) {
                    screen.DrawString(screen.GetCenterX() - 13, screen.GetHeight() - 8, u8"먼저 카드팩을 선택하세요.", FOREGROUND_INTENSITY);
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

                const bool showRoomFrame = (run.currentNodeId < 0) || !IsBattleNodeType(run.currentRoomType);
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
                            audio.PlayEffect(L"battle_victory.wav", L"sfx_victory");

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

                        const int combatPlayerX = screen.GetWidth() * 22 / 100;
                        const int combatEnemyX = screen.GetWidth() * 78 / 100;
                        const int combatBaselineY = screen.GetHeight() - 22;
                        const int combatPlayerY = combatBaselineY;
                        const int combatEnemyY = combatBaselineY;
                        const Rect drawPileRect = { 2, screen.GetHeight() - 10, 10, 4 };
                        const Rect discardPileRect = { screen.GetWidth() - 12, screen.GetHeight() - 10, 10, 4 };
                        const Rect energyRect = { 14, screen.GetHeight() - 10, 16, 4 };
                        Rect intentRect = { combatEnemyX - 11, 8, 24, 5 };

                        auto renderDeadPlayerPose = [&]() {
                            static const std::vector<std::string> deadLines = BuildDeathPlayerArt();
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
                            const int overlapChars = cardCount >= 5 ? 2 : (cardCount >= 2 ? 1 : 0);
                            const int cardSpacing = 28 - overlapChars;
                            const int totalHandWidth = cardCount > 0 ? (28 + (cardCount - 1) * cardSpacing) : 0;
                            const int handStartX = (screen.GetWidth() - totalHandWidth) / 2;
                            const int handBaseY = screen.GetHeight() - 18;

                            vector<CardUI> handCards;
                            handCards.reserve(hand.size());
                            for (size_t cardIndex = 0; cardIndex < hand.size(); ++cardIndex) {
                                handCards.emplace_back(handStartX + static_cast<int>(cardIndex) * cardSpacing, handBaseY, const_cast<CardData*>(&hand[cardIndex]));
                                handCards.back().SetBasePosition(handStartX + static_cast<int>(cardIndex) * cardSpacing, handBaseY);
                                handCards.back().SetRightOcclusion(cardIndex + 1 < hand.size() ? overlapChars : 0);
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
                                const bool hovered = (draggedHandIndex < 0 && cardIndex == hoveredHandIndex);
                                cardUi.SetHovered(hovered || cardIndex == draggedHandIndex);
                                if (hovered || cardIndex == draggedHandIndex) {
                                    cardUi.SetPosition(cardUi.GetX(), handBaseY - 3);
                                }
                            }

                            if (draggedHandIndex >= cardCount) {
                                draggedHandIndex = -1;
                                targetingArrow.SetActive(false);
                            }

                            if (draggedHandIndex < 0 && input.IsLeftClickDown() && hoveredHandIndex >= 0 && run.overlay == RunOverlayType::None) {
                                draggedHandIndex = hoveredHandIndex;
                            }

                            CombatDropTarget dropTarget = CombatDropTarget::None;
                            if (draggedHandIndex >= 0 && draggedHandIndex < cardCount) {
                                const CardData& draggedCard = hand[static_cast<size_t>(draggedHandIndex)];
                                if (discardPileRect.Contains(mouseX, mouseY)) {
                                    dropTarget = CombatDropTarget::DiscardPile;
                                }
                                else if (draggedCard.targetType == CardTargetType::Enemy && enemyEntityUi && enemyEntityUi->IsPointInside(mouseX, mouseY)) {
                                    dropTarget = CombatDropTarget::Enemy;
                                }
                                else if (draggedCard.targetType == CardTargetType::Self && playerEntityUi && playerEntityUi->IsPointInside(mouseX, mouseY)) {
                                    dropTarget = CombatDropTarget::Player;
                                }

                                targetingArrow.SetActive(dropTarget != CombatDropTarget::DiscardPile);
                                targetingArrow.SetStartPoint(handCards[static_cast<size_t>(draggedHandIndex)].GetX() + 14, handBaseY - 1);
                                targetingArrow.SetEndPoint(mouseX, mouseY);
                            }
                            else {
                                targetingArrow.SetActive(false);
                            }

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
                                potionButtons[potionIndex].Render(screen);
                                if (potionButtons[potionIndex].IsClicked()) {
                                    const PotionData potion = run.potions[potionIndex];
                                    if (potion.name == u8"회복 포션") {
                                        run.player.currentHp = (std::min)(run.player.maxHp, run.player.currentHp + 12);
                                        audio.PlayEffect(L"potion_use.wav", L"sfx_potion");
                                        run.potions.erase(run.potions.begin() + static_cast<vector<PotionData>::difference_type>(potionIndex));
                                        break;
                                    }
                                    if (potion.name == u8"에너지 포션") {
                                        combatSystem->AddEnergy(2);
                                        audio.PlayEffect(L"potion_use.wav", L"sfx_potion");
                                        run.potions.erase(run.potions.begin() + static_cast<vector<PotionData>::difference_type>(potionIndex));
                                        break;
                                    }
                                    if (potion.name == u8"연막 포션" && run.currentRoomType != RunNodeType::Boss) {
                                        audio.PlayEffect(L"battle_escape.wav", L"sfx_escape");
                                        run.potions.erase(run.potions.begin() + static_cast<vector<PotionData>::difference_type>(potionIndex));
                                        escapeRequested = true;
                                        break;
                                    }
                                }
                            }

                            if (draggedHandIndex >= 0 && input.IsLeftClickUp() && run.overlay == RunOverlayType::None) {
                                bool actionSucceeded = false;
                                if (dropTarget == CombatDropTarget::DiscardPile) {
                                    CombatActionResult actionResult = combatSystem->TryDiscardCard(draggedHandIndex);
                                    actionSucceeded = actionResult.success;
                                    if (actionResult.success) {
                                        ++globalStats.totalCardsDiscarded;
                                        if (enemyEntityUi && actionResult.enemyHit) {
                                            enemyEntityUi->TriggerHitAnimation();
                                        }
                                        audio.PlayEffect(L"card_discard.wav", L"sfx_discard");
                                    }
                                }
                                else if (dropTarget == CombatDropTarget::Enemy || dropTarget == CombatDropTarget::Player) {
                                    CombatActionResult actionResult = combatSystem->TryUseCard(draggedHandIndex, dropTarget);
                                    actionSucceeded = actionResult.success;
                                    if (actionResult.success) {
                                        ++globalStats.totalCardsUsed;
                                        if (enemyEntityUi && actionResult.enemyHit) {
                                            enemyEntityUi->TriggerHitAnimation();
                                        }
                                        if (playerEntityUi && actionResult.playerHit) {
                                            playerEntityUi->TriggerHitAnimation();
                                        }
                                        audio.PlayEffect(L"card_use.wav", L"sfx_card_use");
                                    }
                                    else if (actionResult.message == "Not enough energy") {
                                        audio.PlayEffect(L"energy_fail.wav", L"sfx_energy_fail");
                                    }
                                }

                                if (!actionSucceeded && dropTarget == CombatDropTarget::DiscardPile) {
                                    audio.PlayEffect(L"card_discard.wav", L"sfx_discard");
                                }

                                draggedHandIndex = -1;
                                targetingArrow.SetActive(false);
                            }

                            const bool allowCombatTime = (run.overlay == RunOverlayType::None);
                            const float combatTimeScale = allowCombatTime
                                ? (draggedHandIndex >= 0 ? combatSystem->GetDragTimeScale() : 1.0f)
                                : 0.0f;
                            const CombatFrameResult frameResult = combatSystem->Update(deltaTimeSec, combatTimeScale * (settings.gameSpeedPercent / 100.0f));

                            if (frameResult.enemyHit && enemyEntityUi) {
                                enemyEntityUi->TriggerHitAnimation();
                            }
                            if (frameResult.playerHit && playerEntityUi) {
                                playerEntityUi->TriggerHitAnimation();
                            }
                            if (frameResult.overdrawRejected) {
                                audio.PlayEffect(L"energy_fail.wav", L"sfx_overdraw");
                            }

                            RenderFrameBox(screen, drawPileRect, COLOR_WHITE);
                            screen.DrawString(drawPileRect.x + 2, drawPileRect.y + 1, u8"뽑기", COLOR_WHITE);
                            screen.DrawString(drawPileRect.x + 2, drawPileRect.y + 2, string("Count ") + to_string(combatSystem->GetDrawPileCount()), COLOR_WHITE);

                            RenderFrameBox(screen, discardPileRect, dropTarget == CombatDropTarget::DiscardPile ? COLOR_YELLOW : COLOR_WHITE);
                            screen.DrawString(discardPileRect.x + 1, discardPileRect.y + 1, u8"버리기", dropTarget == CombatDropTarget::DiscardPile ? COLOR_YELLOW : COLOR_WHITE);
                            screen.DrawString(discardPileRect.x + 2, discardPileRect.y + 2, string("Count ") + to_string(combatSystem->GetDiscardPileCount()), COLOR_WHITE);

                            RenderFrameBox(screen, energyRect, COLOR_YELLOW);
                            screen.DrawString(energyRect.x + 2, energyRect.y + 1, string(u8"에너지 ") + to_string(combatSystem->GetEnergy()) + "/" + to_string(combatSystem->GetMaxEnergy()), COLOR_YELLOW);
                            screen.DrawString(energyRect.x + 2, energyRect.y + 2, string(u8"속도 x") + FormatFloat(combatSystem->GetSpeedMultiplier(), 2), COLOR_GREEN);

                            RenderFrameBox(screen, intentRect, COLOR_RED);
                            const EnemyIntentState& intent = combatSystem->GetCurrentIntent();
                            const string intentHeader = intent.type == EnemyIntentType::Attack
                                ? u8"[ATK] 공격 예고"
                                : (intent.type == EnemyIntentType::Defend ? u8"[BUF] 강화 예고" : u8"[BUF] 강화 예고");
                            screen.DrawString(intentRect.x + 2, intentRect.y + 1, intentHeader, COLOR_RED);
                            screen.DrawString(intentRect.x + 2, intentRect.y + 2, string(u8"피해/수치 ") + to_string(intent.value), COLOR_WHITE);
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

                            for (int cardIndex = 0; cardIndex < cardCount; ++cardIndex) {
                                if (cardIndex == draggedHandIndex || (draggedHandIndex < 0 && cardIndex == hoveredHandIndex)) {
                                    continue;
                                }
                                handCards[static_cast<size_t>(cardIndex)].Render(screen);
                            }
                            if (draggedHandIndex < 0 && hoveredHandIndex >= 0 && hoveredHandIndex < cardCount) {
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
                                    audio.PlayEffect(L"battle_defeat.wav", L"sfx_defeat");
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
                            RenderFrameBox(screen, rewardRect, COLOR_YELLOW);
                            RenderPanelTitle(screen, rewardRect, rewards.title, COLOR_YELLOW);

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
                                        audio.PlayEffect(L"card_pick_sfx.wav", L"sfx_card_pick");
                                        break;
                                    }
                                }

                                ButtonUI btnSkip(rewardRect.x + rewardRect.width / 2 - 8, rewardRect.y + rewardRect.height - 4, 16, 3, u8"넘기기", COLOR_WHITE, COLOR_YELLOW);
                                if (run.overlay == RunOverlayType::None) {
                                    btnSkip.Update(input);
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
                                    }
                                    btnGold.Render(screen);
                                    if (btnGold.IsClicked()) {
                                        run.gold += rewards.goldAmount;
                                        rewards.goldClaimed = true;
                                        SaveManager::SaveContinueRun(run);
                                    }
                                    rewardButtonY += 5;
                                }

                                if (rewards.potionAvailable && !rewards.potionClaimed) {
                                    ButtonUI btnPotion(rewardRect.x + 4, rewardButtonY, rewardRect.width - 8, 4, rewards.potion.name, COLOR_WHITE, COLOR_YELLOW);
                                    if (run.overlay == RunOverlayType::None) {
                                        btnPotion.Update(input);
                                    }
                                    btnPotion.Render(screen);
                                    screen.DrawString(rewardRect.x + 6, rewardButtonY + 1, BuildPotionActionText(rewards.potion), FOREGROUND_INTENSITY);
                                    if (btnPotion.IsClicked()) {
                                        if (HasPotionSlot(run)) {
                                            run.potions.push_back(rewards.potion);
                                            rewards.potionClaimed = true;
                                            SaveManager::SaveContinueRun(run);
                                        }
                                    }
                                    rewardButtonY += 5;
                                }

                                if (rewards.cardRewardAvailable && !rewards.cardRewardClaimed) {
                                    ButtonUI btnCard(rewardRect.x + 4, rewardButtonY, rewardRect.width - 8, 4, u8"덱에 카드를 추가", COLOR_WHITE, COLOR_YELLOW);
                                    if (run.overlay == RunOverlayType::None) {
                                        btnCard.Update(input);
                                    }
                                    btnCard.Render(screen);
                                    if (btnCard.IsClicked()) {
                                        rewards.cardSelectionOpen = true;
                                    }
                                    rewardButtonY += 5;
                                }

                                RenderWrappedText(screen, rewardRect.x + 4, rewardRect.y + rewardRect.height - 9, rewardRect.width - 8, rewards.message, COLOR_WHITE);
                                ButtonUI btnSkipRewards(rewardRect.x + rewardRect.width / 2 - 10, rewardRect.y + rewardRect.height - 4, 20, 3, u8"보상 넘기기", COLOR_WHITE, COLOR_YELLOW);
                                if (run.overlay == RunOverlayType::None) {
                                    btnSkipRewards.Update(input);
                                }
                                btnSkipRewards.Render(screen);
                                if (btnSkipRewards.IsClicked()) {
                                    run.currentRoomSummaryTitle = rewards.title;
                                    run.currentRoomSummaryText = rewards.message;
                                    queueMapReturn();
                                }
                            }
                        }
                        break;
                    }
                    case RunNodeType::Shop:
                    {
                        RenderWrappedText(screen, roomPanel.x + 28, roomPanel.y + 20, roomPanel.width - 34, run.shopRoom.noticeText, COLOR_YELLOW);

                        const int offerWidth = 26;
                        const int offerHeight = 6;
                        const int offerTop = roomPanel.y + 22;
                        const int offerLeft = roomPanel.x + 28;

                        for (size_t offerIndex = 0; offerIndex < run.shopRoom.offers.size(); ++offerIndex) {
                            const int column = static_cast<int>(offerIndex % 3);
                            const int row = static_cast<int>(offerIndex / 3);
                            const Rect offerRect = { offerLeft + column * 28, offerTop + row * 7, offerWidth, offerHeight };
                            ShopOfferState& offer = run.shopRoom.offers[offerIndex];
                            const bool hovered = offerRect.Contains(mouseX, mouseY);
                            const WORD frameColor = offer.sold ? FOREGROUND_INTENSITY : (hovered ? COLOR_YELLOW : COLOR_WHITE);

                            RenderFrameBox(screen, offerRect, frameColor);
                            screen.DrawString(offerRect.x + 2, offerRect.y + 1, offer.title, frameColor);
                            screen.DrawString(offerRect.x + 2, offerRect.y + 2, string(u8"가격 ") + to_string(offer.price) + "G", COLOR_YELLOW);
                            RenderWrappedText(screen, offerRect.x + 2, offerRect.y + 3, offerRect.width - 4, offer.sold ? u8"구매 완료" : offer.description, COLOR_WHITE);

                            if (!offer.sold && hovered && input.IsLeftClickDown()) {
                                if (run.gold < offer.price) {
                                    run.shopRoom.noticeText = u8"골드가 부족합니다.";
                                }
                                else if (offer.type == ShopOfferType::Potion && !HasPotionSlot(run)) {
                                    run.shopRoom.noticeText = u8"포션 칸이 가득 찼습니다.";
                                }
                                else {
                                    run.gold -= offer.price;
                                    offer.sold = true;
                                    switch (offer.type) {
                                    case ShopOfferType::Card:
                                        run.deck.push_back(offer.card);
                                        run.shopRoom.noticeText = offer.card.name + u8" 카드를 구매했습니다.";
                                        break;
                                    case ShopOfferType::Relic:
                                        run.relics.push_back(offer.relic);
                                        run.shopRoom.noticeText = offer.relic.name + u8" 유물을 구매했습니다.";
                                        break;
                                    case ShopOfferType::Potion:
                                        run.potions.push_back(offer.potion);
                                        run.shopRoom.noticeText = offer.potion.name + u8" 포션을 구매했습니다.";
                                        break;
                                    default:
                                        break;
                                    }
                                }
                            }
                        }

                        ButtonUI btnRemove(roomPanel.x + 28, roomPanel.y + roomPanel.height - 5, 20, 3, u8"카드 제거", COLOR_WHITE, COLOR_YELLOW);
                        ButtonUI btnExit(roomPanel.x + 52, roomPanel.y + roomPanel.height - 5, 20, 3, u8"상점을 나간다", COLOR_WHITE, COLOR_YELLOW);
                        if (run.overlay == RunOverlayType::None) {
                            btnRemove.Update(input);
                            btnExit.Update(input);
                        }
                        btnRemove.Render(screen);
                        btnExit.Render(screen);

                        screen.DrawString(roomPanel.x + 28, roomPanel.y + roomPanel.height - 7, string(u8"제거 비용 ") + to_string(run.shopRoom.removalPrice) + "G", FOREGROUND_INTENSITY);
                        if (btnRemove.IsClicked()) {
                            if (run.shopRoom.removalUsed) {
                                run.shopRoom.noticeText = u8"이번 상점에서는 이미 카드를 제거했습니다.";
                            }
                            else {
                                run.shopRoom.removeMode = !run.shopRoom.removeMode;
                                run.shopRoom.noticeText = run.shopRoom.removeMode
                                    ? u8"제거할 카드를 하나 선택하세요."
                                    : u8"카드 제거 모드를 종료했습니다.";
                            }
                        }
                        if (btnExit.IsClicked()) {
                            run.currentRoomSummaryTitle = u8"상점";
                            run.currentRoomSummaryText = run.shopRoom.noticeText.empty() ? u8"상점 정리를 마치고 다음 노드로 이동할 수 있습니다." : run.shopRoom.noticeText;
                            ResolveCurrentNode(run, RunNodeResultType::Resolved);
                            queueMapReturn();
                        }

                        if (run.shopRoom.removeMode) {
                            const Rect deckRect = { roomPanel.x + roomPanel.width - 38, roomPanel.y + 20, 32, roomPanel.height - 28 };
                            RenderFrameBox(screen, deckRect, COLOR_YELLOW);
                            RenderPanelTitle(screen, deckRect, u8"제거할 카드", COLOR_YELLOW);

                            for (size_t cardIndex = 0; cardIndex < run.deck.size() && cardIndex < static_cast<size_t>(deckRect.height - 4); ++cardIndex) {
                                const Rect rowRect = { deckRect.x + 2, deckRect.y + 3 + static_cast<int>(cardIndex), deckRect.width - 4, 1 };
                                const bool hovered = rowRect.Contains(mouseX, mouseY);
                                screen.DrawString(rowRect.x, rowRect.y, TextLayout::AlignToWidth(TextLayout::Utf8ToWide(BuildCardSummary(run.deck[cardIndex])), rowRect.width, TextLayout::HorizontalAlign::Left), hovered ? COLOR_YELLOW : COLOR_WHITE);

                                if (hovered && input.IsLeftClickDown()) {
                                    if (run.shopRoom.removalUsed) {
                                        run.shopRoom.noticeText = u8"이번 상점에서는 이미 카드를 제거했습니다.";
                                    }
                                    else if (run.gold < run.shopRoom.removalPrice) {
                                        run.shopRoom.noticeText = u8"카드 제거 비용이 부족합니다.";
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
                                    }
                                    break;
                                }
                            }
                        }
                        break;
                    }
                    case RunNodeType::Rest:
                    {
                        RenderWrappedText(screen, roomPanel.x + 28, roomPanel.y + 20, roomPanel.width - 34, run.restRoom.noticeText, COLOR_YELLOW);

                        if (run.restRoom.resultReady) {
                            RenderWrappedText(screen, roomPanel.x + 28, roomPanel.y + 24, roomPanel.width - 34, run.restRoom.resultText, COLOR_WHITE);
                            ButtonUI btnContinue(roomPanel.x + 28, roomPanel.y + roomPanel.height - 5, 18, 3, u8"계속", COLOR_WHITE, COLOR_YELLOW);
                            if (run.overlay == RunOverlayType::None) {
                                btnContinue.Update(input);
                            }
                            btnContinue.Render(screen);
                            if (btnContinue.IsClicked()) {
                                run.currentRoomSummaryTitle = u8"휴식";
                                run.currentRoomSummaryText = run.restRoom.resultText;
                                ResolveCurrentNode(run, RunNodeResultType::Resolved);
                                queueMapReturn();
                            }
                        }
                        else {
                            ButtonUI btnRest(roomPanel.x + 28, roomPanel.y + 24, 18, 3, u8"휴식", COLOR_WHITE, COLOR_YELLOW);
                            ButtonUI btnSmith(roomPanel.x + 50, roomPanel.y + 24, 18, 3, u8"강화", COLOR_WHITE, COLOR_YELLOW);
                            ButtonUI btnLeave(roomPanel.x + 72, roomPanel.y + 24, 18, 3, u8"나가기", COLOR_WHITE, COLOR_YELLOW);
                            if (run.overlay == RunOverlayType::None) {
                                btnRest.Update(input);
                                btnSmith.Update(input);
                                btnLeave.Update(input);
                            }
                            btnRest.Render(screen);
                            btnSmith.Render(screen);
                            btnLeave.Render(screen);

                            if (btnRest.IsClicked()) {
                                const int healAmount = (std::max)(12, run.player.maxHp / 4);
                                const int beforeHp = run.player.currentHp;
                                run.player.currentHp = (std::min)(run.player.maxHp, run.player.currentHp + healAmount);
                                run.restRoom.resultReady = true;
                                run.restRoom.resultText = string(u8"모닥불 옆에서 숨을 고르며 체력을 ")
                                    + to_string(run.player.currentHp - beforeHp) + u8" 회복했습니다.";
                            }
                            if (btnSmith.IsClicked()) {
                                run.restRoom.noticeText = u8"카드 강화 목록과 실제 강화 효과는 다음 단계에서 연결합니다.";
                            }
                            if (btnLeave.IsClicked()) {
                                run.currentRoomSummaryTitle = u8"휴식";
                                run.currentRoomSummaryText = u8"휴식 지점을 정리하고 다음 노드로 이동할 수 있습니다.";
                                ResolveCurrentNode(run, RunNodeResultType::Resolved);
                                queueMapReturn();
                            }
                        }
                        break;
                    }
                    case RunNodeType::Treasure:
                    {
                        if (!run.treasureRoom.noticeText.empty()) {
                            RenderWrappedText(screen, roomPanel.x + 28, roomPanel.y + 20, roomPanel.width - 34, run.treasureRoom.noticeText, COLOR_YELLOW);
                        }

                        if (run.treasureRoom.choiceCommitted) {
                            RenderWrappedText(screen, roomPanel.x + 28, roomPanel.y + 24, roomPanel.width - 34, run.treasureRoom.resultText, COLOR_WHITE);
                            ButtonUI btnContinue(roomPanel.x + 28, roomPanel.y + roomPanel.height - 5, 18, 3, u8"계속", COLOR_WHITE, COLOR_YELLOW);
                            if (run.overlay == RunOverlayType::None) {
                                btnContinue.Update(input);
                            }
                            btnContinue.Render(screen);
                            if (btnContinue.IsClicked()) {
                                run.currentRoomSummaryTitle = u8"보물";
                                run.currentRoomSummaryText = run.treasureRoom.resultText;
                                ResolveCurrentNode(run, RunNodeResultType::Resolved);
                                queueMapReturn();
                            }
                        }
                        else {
                            const int choiceWidth = 26;
                            const int choiceHeight = 6;
                            const int choiceLeft = roomPanel.x + 28;
                            const int choiceTop = roomPanel.y + 22;
                            for (size_t choiceIndex = 0; choiceIndex < run.treasureRoom.choices.size(); ++choiceIndex) {
                                const int column = static_cast<int>(choiceIndex % 2);
                                const int row = static_cast<int>(choiceIndex / 2);
                                const Rect choiceRect = { choiceLeft + column * 30, choiceTop + row * 7, choiceWidth, choiceHeight };
                                TreasureChoiceState& choice = run.treasureRoom.choices[choiceIndex];
                                const bool hovered = choiceRect.Contains(mouseX, mouseY);
                                RenderFrameBox(screen, choiceRect, hovered ? COLOR_YELLOW : COLOR_WHITE);
                                screen.DrawString(choiceRect.x + 2, choiceRect.y + 1, choice.title, hovered ? COLOR_YELLOW : COLOR_WHITE);
                                RenderWrappedText(screen, choiceRect.x + 2, choiceRect.y + 3, choiceRect.width - 4, choice.description, COLOR_WHITE);

                                if (hovered && input.IsLeftClickDown()) {
                                    if (choice.grantPotion && !HasPotionSlot(run)) {
                                        run.treasureRoom.noticeText = u8"포션 칸이 가득 차서 이 보상은 받을 수 없습니다.";
                                    }
                                    else {
                                        run.treasureRoom.noticeText.clear();
                                        run.treasureRoom.choiceCommitted = true;
                                        run.treasureRoom.selectedChoiceId = choice.id;
                                        if (choice.goldReward > 0) {
                                            run.gold += choice.goldReward;
                                            run.treasureRoom.resultText = string(u8"금화 더미를 챙겨 골드 ") + to_string(choice.goldReward) + u8"을 획득했습니다.";
                                        }
                                        else if (choice.grantRelic) {
                                            run.relics.push_back(choice.relic);
                                            run.treasureRoom.resultText = choice.relic.name + u8" 유물을 획득했습니다.";
                                        }
                                        else if (choice.grantPotion) {
                                            run.potions.push_back(choice.potion);
                                            run.treasureRoom.resultText = choice.potion.name + u8" 포션을 획득했습니다.";
                                        }
                                    }
                                }
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
                    tooltip.SetText(tooltipLines);
                    tooltip.SetVisible(true);
                    tooltip.UpdatePosition(mouseX, mouseY, screen.GetWidth(), screen.GetHeight());

                    const bool canSelectNode = (run.currentNodeId < 0 || run.roomResolved) && CanEnterNode(run, hoveredNodeId);
                    if (canSelectNode && input.IsLeftClickDown()) {
                        EnterNode(run, hoveredNodeId);
                        SaveManager::SaveContinueRun(run);
                        tooltip.SetVisible(false);
                        run.overlay = RunOverlayType::None;
                    }
                }
                else {
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
                Rect masterTrack = { contentLeft + 12, popup.y + 5, contentWidth - 22, 1 };
                Rect bgmTrack = { contentLeft + 12, popup.y + 7, contentWidth - 22, 1 };
                Rect sfxTrack = { contentLeft + 12, popup.y + 9, contentWidth - 22, 1 };
                Rect speedTrack = { contentLeft + 12, popup.y + 11, contentWidth - 22, 1 };

                UpdateSliderDrag(input, mouseX, 0, masterTrack, settings.masterVolume, activeSliderId);
                UpdateSliderDrag(input, mouseX, 1, bgmTrack, settings.bgmVolume, activeSliderId);
                UpdateSliderDrag(input, mouseX, 2, sfxTrack, settings.sfxVolume, activeSliderId);
                UpdateSliderDrag(input, mouseX, 3, speedTrack, settings.gameSpeedPercent, activeSliderId);

                RenderSlider(screen, contentLeft, popup.y + 5, contentWidth, u8"마스터", settings.masterVolume, activeSliderId == 0);
                RenderSlider(screen, contentLeft, popup.y + 7, contentWidth, u8"BGM", settings.bgmVolume, activeSliderId == 1);
                RenderSlider(screen, contentLeft, popup.y + 9, contentWidth, u8"SFX", settings.sfxVolume, activeSliderId == 2);
                RenderSlider(screen, contentLeft, popup.y + 11, contentWidth, u8"속도", settings.gameSpeedPercent, activeSliderId == 3);

                ButtonUI btnToggleDebug(contentLeft, popup.y + 15, 18, 3, string(u8"디버그 ") + (settings.debugMode ? u8"ON" : u8"OFF"), COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnBack(contentLeft, popup.y + popup.height - 4, 16, 3, u8"돌아가기", COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnAbandon(contentLeft + 18, popup.y + popup.height - 4, 18, 3, u8"전투 포기", COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnSaveExit(contentLeft + 38, popup.y + popup.height - 4, 20, 3, u8"저장 후 종료", COLOR_WHITE, COLOR_YELLOW);
                btnToggleDebug.Update(input);
                btnBack.Update(input);
                btnAbandon.Update(input);
                btnSaveExit.Update(input);
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

            if (tooltip.IsVisible()) {
                tooltip.Render(screen);
            }

            const int debugBaseY = screen.GetHeight() - 5;
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
