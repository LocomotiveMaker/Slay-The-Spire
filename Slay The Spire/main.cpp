// -----------------------------------------------------------------------------
// @file       main.cpp
// @brief      Application entry point for the title/run state machine skeleton.
// -----------------------------------------------------------------------------
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "AudioManager.h"
#include "ButtonUI.h"
#include "InputManager.h"
#include "MapRenderer.h"
#include "RunState.h"
#include "SaveManager.h"
#include "ScreenManager.h"
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
    audio.PlayBGM(L"Exordium.wav", 100.0f);

    AppState appState = AppState::Title;
    TitleOverlayType titleOverlay = TitleOverlayType::None;
    RunStateData run = {};
    bool hasContinueRun = SaveManager::HasContinueRun();
    bool shouldQuit = false;
    bool showRecordsAfterEnding = false;
    bool endingWon = false;
    string endingTitle;
    string endingBody;
    int selectedRecordIndex = 0;
    int recordsScroll = 0;
    int deckScroll = 0;
    int activeSliderId = -1;
    float runPlayAccumulatorSec = 0.0f;
    bool settingsDirty = false;

    bool wasF6Pressed = false;
    bool wasF7Pressed = false;
    bool wasF8Pressed = false;
    bool wasF9Pressed = false;
    bool wasF10Pressed = false;
    bool wasF11Pressed = false;

    auto reloadRecords = [&]() {
        runRecords = SaveManager::LoadRunRecords();
        selectedRecordIndex = (std::min)(selectedRecordIndex, (std::max)(0, static_cast<int>(runRecords.size()) - 1));
        if (selectedRecordIndex < 0) {
            selectedRecordIndex = 0;
        }
        };

    auto startNewRun = [&]() {
        CreateNewRun(run, BuildSeed(settings), screen.GetWidth(), screen.GetHeight());
        SaveManager::SaveContinueRun(run);
        hasContinueRun = true;
        mapRenderer.SetNodes(&run.nodes);
        mapRenderer.FocusToFloor(1);
        tooltip.SetVisible(false);
        appState = AppState::Run;
        titleOverlay = TitleOverlayType::None;
        deckScroll = 0;
        runPlayAccumulatorSec = 0.0f;
        audio.QueueBGMFade(L"Exordium.wav", 50.0f, 0.25f, 0.45f);
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
        tooltip.SetVisible(false);
        appState = AppState::Run;
        titleOverlay = TitleOverlayType::None;
        deckScroll = 0;
        runPlayAccumulatorSec = 0.0f;
        audio.QueueBGMFade(L"Exordium.wav", 50.0f, 0.25f, 0.45f);
        return true;
        };

    auto transitionToTitle = [&]() {
        appState = AppState::Title;
        run = {};
        titleOverlay = showRecordsAfterEnding ? TitleOverlayType::Records : TitleOverlayType::None;
        showRecordsAfterEnding = false;
        mapRenderer.SetNodes(nullptr);
        tooltip.SetVisible(false);
        hasContinueRun = SaveManager::HasContinueRun();
        audio.QueueBGMFade(L"Exordium.wav", 100.0f, 0.25f, 0.4f);
        };

    auto finishRunToEnding = [&](bool won, const string& failureReasonText) {
        endingWon = won;
        endingTitle = won ? u8"프로토타입 엔딩" : u8"프로토타입 패배";
        endingBody = won
            ? (string(u8"보스를 쓰러뜨렸습니다.\n시드 ") + to_string(run.seed) + "\n플레이 시간 " + FormatPlayTime(run.playTimeSec))
            : failureReasonText;

        run.finished = true;
        run.won = won;

        if (!run.loseRecordCommitted || won) {
            RunRecordData record = BuildRunRecord(run, won, failureReasonText);
            SaveManager::AppendRunRecord(record, globalStats);
            reloadRecords();
            run.loseRecordCommitted = true;
        }

        SaveManager::DeleteContinueRun();
        hasContinueRun = false;
        showRecordsAfterEnding = true;
        tooltip.SetVisible(false);
        activeSliderId = -1;
        run.pendingConfirm = ConfirmActionType::None;
        run.overlay = RunOverlayType::Ending;
        appState = AppState::Run;
        };

    auto abandonContinueRunFromTitle = [&]() {
        RunStateData loadedRun = {};
        if (SaveManager::LoadContinueRun(loadedRun)) {
            const string failureReason = to_string((std::max)(1, loadedRun.currentFloor)) + u8"층에서 도전을 포기하셨습니다.";
            RunRecordData record = BuildRunRecord(loadedRun, false, failureReason);
            SaveManager::AppendRunRecord(record, globalStats);
            reloadRecords();
        }
        SaveManager::DeleteContinueRun();
        hasContinueRun = false;
        titleOverlay = TitleOverlayType::Records;
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
        const bool pressedF6 = ConsumeKeyPress(VK_F6, wasF6Pressed);
        const bool pressedF7 = ConsumeKeyPress(VK_F7, wasF7Pressed);
        const bool pressedF8 = ConsumeKeyPress(VK_F8, wasF8Pressed);
        const bool pressedF9 = ConsumeKeyPress(VK_F9, wasF9Pressed);
        const bool pressedF10 = ConsumeKeyPress(VK_F10, wasF10Pressed);
        const bool pressedF11 = ConsumeKeyPress(VK_F11, wasF11Pressed);

        if (appState == AppState::Run && settings.debugMode && run.overlay == RunOverlayType::None) {
            const auto openDebugRoom = [&](RunNodeType type) {
                run.currentRoomType = type;
                run.currentRoomResult = RunNodeResultType::None;
                run.roomResolved = false;
                run.currentNodeId = -1;
                run.scene = RunSceneType::Room;
                ResetRoomRuntimeState(run);
                PrepareCurrentRoomState(run);
                };

            if (pressedF6) openDebugRoom(RunNodeType::Shop);
            if (pressedF7) openDebugRoom(RunNodeType::Rest);
            if (pressedF8) openDebugRoom(RunNodeType::Treasure);
            if (pressedF9) openDebugRoom(RunNodeType::Battle);
            if (pressedF10) openDebugRoom(RunNodeType::Elite);
            if (pressedF11) openDebugRoom(RunNodeType::Boss);
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
                const Rect popup = { 18, 6, screen.GetWidth() - 36, screen.GetHeight() - 12 };
                RenderFrameBox(screen, popup, COLOR_YELLOW);
                RenderPanelTitle(screen, popup, u8"설정", COLOR_YELLOW);

                const int contentLeft = popup.x + 4;
                const int contentWidth = popup.width - 8;
                Rect masterTrack = { contentLeft + 14, popup.y + 5, contentWidth - 18, 1 };
                Rect bgmTrack = { contentLeft + 14, popup.y + 7, contentWidth - 18, 1 };
                Rect sfxTrack = { contentLeft + 14, popup.y + 9, contentWidth - 18, 1 };
                Rect speedTrack = { contentLeft + 14, popup.y + 11, contentWidth - 18, 1 };

                UpdateSliderDrag(input, mouseX, 0, masterTrack, settings.masterVolume, activeSliderId);
                UpdateSliderDrag(input, mouseX, 1, bgmTrack, settings.bgmVolume, activeSliderId);
                UpdateSliderDrag(input, mouseX, 2, sfxTrack, settings.sfxVolume, activeSliderId);
                UpdateSliderDrag(input, mouseX, 3, speedTrack, settings.gameSpeedPercent, activeSliderId);

                RenderSlider(screen, contentLeft, popup.y + 5, contentWidth, u8"마스터", settings.masterVolume, activeSliderId == 0);
                RenderSlider(screen, contentLeft, popup.y + 7, contentWidth, u8"BGM", settings.bgmVolume, activeSliderId == 1);
                RenderSlider(screen, contentLeft, popup.y + 9, contentWidth, u8"SFX", settings.sfxVolume, activeSliderId == 2);
                RenderSlider(screen, contentLeft, popup.y + 11, contentWidth, u8"속도", settings.gameSpeedPercent, activeSliderId == 3);

                ButtonUI btnToggleEffects(contentLeft, popup.y + 14, 22, 3, string(u8"이펙트 ") + (settings.effectsEnabled ? u8"ON" : u8"OFF"), COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnToggleDebug(contentLeft + 24, popup.y + 14, 22, 3, string(u8"디버그 ") + (settings.debugMode ? u8"ON" : u8"OFF"), COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnToggleChar(contentLeft, popup.y + 18, 22, 3, string(u8"캐릭 선택 ") + (settings.enableCharacterSelect ? u8"ON" : u8"OFF"), COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnToggleAsc(contentLeft + 24, popup.y + 18, 22, 3, string(u8"승천 선택 ") + (settings.enableAscensionSelect ? u8"ON" : u8"OFF"), COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnClose(popup.x + popup.width - 20, popup.y + popup.height - 5, 16, 3, u8"닫기", COLOR_WHITE, COLOR_YELLOW);

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

                screen.DrawString(contentLeft, popup.y + 22, u8"커스텀 시드 입력: 추후 구현 예정", FOREGROUND_INTENSITY);

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

                if (pressedEsc) {
                    titleOverlay = TitleOverlayType::None;
                }

                if (!runRecords.empty()) {
                    const int listTop = popup.y + 5;
                    const int visibleCount = popup.height - 10;
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

                    const vector<string> detailLines = {
                        string(u8"날짜: ") + selectedRecord.timestampText,
                        string(u8"시드: ") + to_string(selectedRecord.seed),
                        string(u8"플레이 시간: ") + FormatPlayTime(selectedRecord.playTimeSec),
                        string(u8"도달 층: ") + to_string(selectedRecord.reachedFloor),
                        selectedRecord.failureReasonText.empty() ? u8"패배 원인: 없음" : (string(u8"패배 원인: ") + selectedRecord.failureReasonText),
                        string(u8"유물: ") + BuildRelicListText(selectedRecord.relics),
                        string(u8"방문 노드: ") + BuildNodeVisitedText(selectedRecord.visitedNodes),
                        string(u8"덱 장수: ") + to_string(static_cast<int>(selectedRecord.deckSnapshot.size()))
                    };

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
            const string floorText = string(u8"층: ") + to_string((std::max)(1, run.currentFloor));
            screen.DrawString(2, 1, run.playerName, COLOR_WHITE);
            screen.DrawString(screen.GetCenterX() - 8, 1, hpText, COLOR_RED);
            screen.DrawString(screen.GetCenterX() + 16, 1, goldText, COLOR_YELLOW);
            screen.DrawString(screen.GetWidth() - 18, 1, floorText, COLOR_WHITE);
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
                const Rect headline = { screen.GetCenterX() - 32, 7, 64, 5 };
                const Rect detailRect = { screen.GetCenterX() - 44, 39, 88, 14 };
                RenderFrameBox(screen, headline, COLOR_YELLOW);
                RenderPanelTitle(screen, headline, u8"시너지 카드팩 선택", COLOR_YELLOW);
                RenderWrappedText(screen, headline.x + 3, headline.y + 3, headline.width - 6, u8"첫 시작에서는 반드시 3개 중 하나의 카드팩을 고른 뒤 확인 버튼으로 런을 시작합니다.", COLOR_WHITE);

                int previewIndex = run.selectedStarterPackIndex;
                for (size_t packIndex = 0; packIndex < starterPacks.size(); ++packIndex) {
                    const int panelWidth = 28;
                    const int panelHeight = 20;
                    const int startX = screen.GetCenterX() - 46 + static_cast<int>(packIndex) * 31;
                    const Rect packRect = { startX, 15, panelWidth, panelHeight };
                    const bool hovered = packRect.Contains(mouseX, mouseY);
                    const bool selected = (run.selectedStarterPackIndex == static_cast<int>(packIndex));
                    const WORD frameColor = selected || hovered ? starterPacks[packIndex].accentColor : COLOR_WHITE;

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
                    if (hovered) {
                        previewIndex = static_cast<int>(packIndex);
                    }
                    if (hovered && input.IsLeftClickDown()) {
                        run.selectedStarterPackIndex = static_cast<int>(packIndex);
                    }
                }

                if (previewIndex >= 0 && previewIndex < static_cast<int>(starterPacks.size())) {
                    const CardPackOption& previewPack = starterPacks[static_cast<size_t>(previewIndex)];
                    vector<string> previewLines;
                    previewLines.push_back(previewPack.description);
                    previewLines.push_back(string(u8"카드 수: ") + to_string(previewPack.cards.size()));
                    previewLines.push_back(string(u8"추천 성향: ") + (previewIndex == 0 ? u8"공격 템포" : (previewIndex == 1 ? u8"안정적인 운영" : u8"유틸과 변칙")));
                    previewLines.push_back(u8"추가되는 카드:");
                    for (const CardData& card : previewPack.cards) {
                        previewLines.push_back(string(" - ") + BuildCardSummary(card) + " / " + card.description);
                    }

                    RenderFrameBox(screen, detailRect, previewPack.accentColor);
                    RenderPanelTitle(screen, detailRect, previewPack.title, previewPack.accentColor);
                    RenderTextBlock(screen, { detailRect.x + 3, detailRect.y + 4, detailRect.width - 6, detailRect.height - 5 }, previewLines, COLOR_WHITE);
                }
                else {
                    RenderFrameBox(screen, detailRect, COLOR_WHITE);
                    RenderPanelTitle(screen, detailRect, u8"선택 대기", COLOR_WHITE);
                    RenderWrappedText(screen, detailRect.x + 3, detailRect.y + 5, detailRect.width - 6, u8"카드팩 위에 마우스를 올리거나 클릭하면 상세 정보가 표시됩니다.", COLOR_WHITE);
                }

                ButtonUI btnConfirm(screen.GetCenterX() - 12, detailRect.y + detailRect.height + 1, 24, 3, u8"이 카드팩으로 시작", COLOR_WHITE, COLOR_YELLOW);
                if (run.selectedStarterPackIndex >= 0) {
                    btnConfirm.Update(input);
                }
                btnConfirm.Render(screen);
                if (run.selectedStarterPackIndex < 0) {
                    screen.DrawString(screen.GetCenterX() - 12, detailRect.y + detailRect.height - 1, u8"먼저 카드팩을 선택하세요.", FOREGROUND_INTENSITY);
                }
                else if (btnConfirm.IsClicked()) {
                    ApplyStarterPack(run, starterPacks[static_cast<size_t>(run.selectedStarterPackIndex)]);
                    run.scene = RunSceneType::Room;
                    run.overlay = RunOverlayType::Map;
                    SaveManager::SaveContinueRun(run);
                    audio.FadeCurrentBGMTo(50.0f, 0.4f);
                }
            }
            else {
                const Rect roomPanel = { 18, 8, screen.GetWidth() - 36, screen.GetHeight() - 18 };
                RenderFrameBox(screen, roomPanel, COLOR_WHITE);

                string roomTitle;
                string roomBody;
                vector<string> artLines;

                auto queueMapReturn = [&]() {
                    run.overlay = RunOverlayType::Map;
                    tooltip.SetVisible(false);
                    mapRenderer.FocusToFloor(run.currentFloor > 0 ? run.currentFloor : 1);
                    SaveManager::SaveContinueRun(run);
                    };

                if (run.currentNodeId < 0) {
                    roomTitle = u8"다음 노드를 선택하세요";
                    roomBody = u8"지도 오버레이에서 현재 연결된 노드를 클릭하여 런을 진행합니다.";
                    artLines = { u8"<지도>", u8"연결된", u8"노드 클릭" };
                }
                else {
                    PrepareCurrentRoomState(run);
                    roomTitle = RunNodeTypeToDisplayName(run.currentRoomType);
                    roomBody = RunNodeTypeToDescription(run.currentRoomType);

                    switch (run.currentRoomType) {
                    case RunNodeType::Battle:
                    case RunNodeType::Elite:
                    case RunNodeType::Boss:
                        roomBody += u8"\n현재 단계에서는 임시 결과 버튼으로 흐름만 검증합니다.";
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
                }

                RenderPanelTitle(screen, roomPanel, roomTitle, COLOR_YELLOW);
                const Rect artRect = { roomPanel.x + 3, roomPanel.y + 5, 22, roomPanel.height - 11 };
                RenderFrameBox(screen, artRect, FOREGROUND_INTENSITY);
                RenderAsciiArtLines(screen, artRect, artLines, COLOR_WHITE);
                RenderWrappedText(screen, roomPanel.x + 28, roomPanel.y + 6, roomPanel.width - 32, roomBody, COLOR_WHITE);

                screen.DrawString(roomPanel.x + 28, roomPanel.y + 13, string(u8"시드 ") + to_string(run.seed), FOREGROUND_INTENSITY);
                screen.DrawString(roomPanel.x + 28, roomPanel.y + 15, string(u8"유물: ") + BuildRelicListText(run.relics), FOREGROUND_INTENSITY);
                screen.DrawString(roomPanel.x + 28, roomPanel.y + 17, string(u8"포션: ") + BuildPotionListText(run.potions), FOREGROUND_INTENSITY);

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
                else {
                    switch (run.currentRoomType) {
                    case RunNodeType::Battle:
                    case RunNodeType::Elite:
                    {
                        ButtonUI btnWin(roomPanel.x + 6, roomPanel.y + roomPanel.height - 6, 18, 3, u8"임시 승리", COLOR_WHITE, COLOR_YELLOW);
                        ButtonUI btnLose(roomPanel.x + 28, roomPanel.y + roomPanel.height - 6, 18, 3, u8"임시 패배", COLOR_WHITE, COLOR_YELLOW);
                        if (run.overlay == RunOverlayType::None) {
                            btnWin.Update(input);
                            btnLose.Update(input);
                        }
                        btnWin.Render(screen);
                        btnLose.Render(screen);
                        if (btnWin.IsClicked()) {
                            ResolveCurrentNode(run, RunNodeResultType::Victory);
                            queueMapReturn();
                        }
                        if (btnLose.IsClicked()) {
                            finishRunToEnding(false, to_string(run.currentFloor) + u8"층 전투에서 쓰러졌습니다.");
                        }
                        break;
                    }
                    case RunNodeType::Boss:
                    {
                        ButtonUI btnWin(roomPanel.x + 6, roomPanel.y + roomPanel.height - 6, 18, 3, u8"임시 승리", COLOR_WHITE, COLOR_YELLOW);
                        ButtonUI btnLose(roomPanel.x + 28, roomPanel.y + roomPanel.height - 6, 18, 3, u8"임시 패배", COLOR_WHITE, COLOR_YELLOW);
                        if (run.overlay == RunOverlayType::None) {
                            btnWin.Update(input);
                            btnLose.Update(input);
                        }
                        btnWin.Render(screen);
                        btnLose.Render(screen);
                        if (btnWin.IsClicked()) {
                            ResolveCurrentNode(run, RunNodeResultType::Victory);
                            finishRunToEnding(true, "");
                        }
                        if (btnLose.IsClicked()) {
                            finishRunToEnding(false, to_string(run.currentFloor) + u8"층 보스전에서 쓰러졌습니다.");
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
                                ResolveCurrentNode(run, RunNodeResultType::Resolved);
                                queueMapReturn();
                            }
                        }
                        else {
                            const int buttonWidth = (std::max)(36, roomPanel.width / 3);
                            const int buttonHeight = 4;
                            const int buttonX = roomPanel.x + roomPanel.width - buttonWidth - 8;
                            const int buttonStartY = roomPanel.y + roomPanel.height - 6 - static_cast<int>(run.eventRoom.choices.size()) * (buttonHeight + 1);
                            for (size_t choiceIndex = 0; choiceIndex < run.eventRoom.choices.size(); ++choiceIndex) {
                                EventChoiceState& choice = run.eventRoom.choices[choiceIndex];
                                const int buttonY = buttonStartY + static_cast<int>(choiceIndex) * (buttonHeight + 1);
                                ButtonUI btnChoice(buttonX, buttonY, buttonWidth, buttonHeight, BuildEventChoiceButtonText(choice), COLOR_WHITE, COLOR_YELLOW);
                                if (run.overlay == RunOverlayType::None) {
                                    btnChoice.Update(input);
                                }
                                btnChoice.Render(screen);

                                if (btnChoice.IsClicked()) {
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

            if (run.overlay == RunOverlayType::Map) {
                const Rect overlayRect = { 6, 5, screen.GetWidth() - 12, screen.GetHeight() - 10 };
                RenderFrameBox(screen, overlayRect, COLOR_YELLOW);
                RenderPanelTitle(screen, overlayRect, u8"지도", COLOR_YELLOW);
                screen.DrawString(overlayRect.x + 3, overlayRect.y + 2, u8"M 또는 ESC로 닫기 / 휠로 스크롤 / 클릭으로 진입", FOREGROUND_INTENSITY);

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
                const Rect popup = { 18, 6, screen.GetWidth() - 36, screen.GetHeight() - 12 };
                RenderFrameBox(screen, popup, COLOR_YELLOW);
                RenderPanelTitle(screen, popup, u8"인게임 설정", COLOR_YELLOW);

                const int contentLeft = popup.x + 4;
                const int contentWidth = popup.width - 8;
                Rect masterTrack = { contentLeft + 14, popup.y + 5, contentWidth - 18, 1 };
                Rect bgmTrack = { contentLeft + 14, popup.y + 7, contentWidth - 18, 1 };
                Rect sfxTrack = { contentLeft + 14, popup.y + 9, contentWidth - 18, 1 };
                Rect speedTrack = { contentLeft + 14, popup.y + 11, contentWidth - 18, 1 };

                UpdateSliderDrag(input, mouseX, 0, masterTrack, settings.masterVolume, activeSliderId);
                UpdateSliderDrag(input, mouseX, 1, bgmTrack, settings.bgmVolume, activeSliderId);
                UpdateSliderDrag(input, mouseX, 2, sfxTrack, settings.sfxVolume, activeSliderId);
                UpdateSliderDrag(input, mouseX, 3, speedTrack, settings.gameSpeedPercent, activeSliderId);

                RenderSlider(screen, contentLeft, popup.y + 5, contentWidth, u8"마스터", settings.masterVolume, activeSliderId == 0);
                RenderSlider(screen, contentLeft, popup.y + 7, contentWidth, u8"BGM", settings.bgmVolume, activeSliderId == 1);
                RenderSlider(screen, contentLeft, popup.y + 9, contentWidth, u8"SFX", settings.sfxVolume, activeSliderId == 2);
                RenderSlider(screen, contentLeft, popup.y + 11, contentWidth, u8"속도", settings.gameSpeedPercent, activeSliderId == 3);

                ButtonUI btnToggleDebug(contentLeft, popup.y + 14, 22, 3, string(u8"디버그 ") + (settings.debugMode ? u8"ON" : u8"OFF"), COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnBack(contentLeft, popup.y + popup.height - 5, 16, 3, u8"돌아가기", COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnAbandon(contentLeft + 20, popup.y + popup.height - 5, 18, 3, u8"전투 포기", COLOR_WHITE, COLOR_YELLOW);
                ButtonUI btnSaveExit(contentLeft + 42, popup.y + popup.height - 5, 20, 3, u8"저장 후 종료", COLOR_WHITE, COLOR_YELLOW);
                btnToggleDebug.Update(input);
                btnBack.Update(input);
                btnAbandon.Update(input);
                btnSaveExit.Update(input);
                btnToggleDebug.Render(screen);
                btnBack.Render(screen);
                btnAbandon.Render(screen);
                btnSaveExit.Render(screen);
                screen.DrawString(contentLeft + 26, popup.y + 14, u8"키 설정: 틀만 준비", FOREGROUND_INTENSITY);

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
            else if (run.overlay == RunOverlayType::Ending) {
                const Rect panel = { screen.GetCenterX() - 28, screen.GetCenterY() - 8, 56, 16 };
                RenderFrameBox(screen, panel, endingWon ? COLOR_GREEN : COLOR_RED);
                RenderPanelTitle(screen, panel, endingTitle, endingWon ? COLOR_GREEN : COLOR_RED);
                RenderWrappedText(screen, panel.x + 4, panel.y + 5, panel.width - 8, endingBody, COLOR_WHITE);

                ButtonUI btnReturn(panel.x + panel.width / 2 - 8, panel.y + panel.height - 4, 16, 3, u8"타이틀로", COLOR_WHITE, COLOR_YELLOW);
                btnReturn.Update(input);
                btnReturn.Render(screen);
                if (btnReturn.IsClicked()) {
                    transitionToTitle();
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
            const Rect panel = { screen.GetCenterX() - 28, screen.GetCenterY() - 8, 56, 16 };
            RenderFrameBox(screen, panel, endingWon ? COLOR_GREEN : COLOR_RED);
            RenderPanelTitle(screen, panel, endingTitle, endingWon ? COLOR_GREEN : COLOR_RED);
            RenderWrappedText(screen, panel.x + 4, panel.y + 5, panel.width - 8, endingBody, COLOR_WHITE);

            ButtonUI btnReturn(panel.x + panel.width / 2 - 8, panel.y + panel.height - 4, 16, 3, u8"타이틀로", COLOR_WHITE, COLOR_YELLOW);
            btnReturn.Update(input);
            btnReturn.Render(screen);
            if (btnReturn.IsClicked()) {
                transitionToTitle();
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
