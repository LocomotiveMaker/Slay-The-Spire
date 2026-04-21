// -----------------------------------------------------------------------------
// @file       main.cpp
// @brief      Slay The Spire console tech demo entry point.
// -----------------------------------------------------------------------------
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "Animation.h"
#include "AudioManager.h"
#include "ButtonUI.h"
#include "CardUI.h"
#include "CombatSystem.h"
#include "EntityUI.h"
#include "FloatingText.h"
#include "GameData.h"
#include "HUDPanel.h"
#include "InputManager.h"
#include "MapRenderer.h"
#include "ModalPopupUI.h"
#include "ScreenManager.h"
#include "TargetingArrow.h"
#include "TextLayout.h"
#include "TooltipUI.h"

using namespace std;

enum class ViewState { Combat, Map };

const vector<vector<string>> FX_CAMPFIRE = {
    { "  (  ", " ) ( ", "( * )", " /|\\ ", "[===]" },
    { " (   ", "  )  ", "( * )", " /|\\ ", "[===]" },
    { "   ) ", " ( ( ", "( * )", " /|\\ ", "[===]" }
};

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

    if (refreshRate > 240) {
        return 240;
    }

    return refreshRate;
}

string FormatFloat(float value, int precision = 1) {
    char buffer[64] = {};
    sprintf_s(buffer, "%.*f", precision, value);
    return buffer;
}

vector<CardData> BuildPrototypeDeck() {
    vector<CardData> deck;
    int nextId = 0;

    for (int i = 0; i < 4; ++i) {
        deck.push_back({
            nextId++,
            u8"강타",
            1,
            u8"적에게 6 피해를 줍니다.",
            CardType::Attack,
            CardTargetType::Enemy,
            CardEffectType::AttackDamage,
            CardDiscardEffectType::None,
            6,
            0
            });
    }

    for (int i = 0; i < 4; ++i) {
        deck.push_back({
            nextId++,
            u8"수비",
            1,
            u8"방어도 5를 얻습니다.",
            CardType::Skill,
            CardTargetType::Self,
            CardEffectType::DefendBlock,
            CardDiscardEffectType::None,
            5,
            0
            });
    }

    deck.push_back({
        nextId++,
        u8"재정비",
        0,
        u8"버릴 때 카드 1장을 뽑고 에너지 1을 얻습니다.",
        CardType::Skill,
        CardTargetType::None,
        CardEffectType::None,
        CardDiscardEffectType::DrawCardsGainEnergy,
        1,
        1
        });

    deck.push_back({
        nextId++,
        u8"약점 노출",
        2,
        u8"적에게 취약 2를 부여합니다.",
        CardType::Skill,
        CardTargetType::Enemy,
        CardEffectType::ApplyVulnerable,
        CardDiscardEffectType::None,
        2,
        0
        });

    return deck;
}

void ResetPrototypeEntities(EntityData& playerData, EntityData& enemyData) {
    playerData = { 0, u8"아이언클래드", 80, 80, 0, 0, 0, 0, 0 };
    enemyData = { 1, u8"훈련용 슬라임", 48, 48, 0, 0, 0, 0, 0 };
}

Rect MakeDrawPileRect(const ScreenManager& screen) {
    return { 2, screen.GetHeight() - 15, 12, 5 };
}

Rect MakeDiscardPileRect(const ScreenManager& screen) {
    return { screen.GetWidth() - 14, screen.GetHeight() - 15, 12, 5 };
}

Rect ExpandRect(const Rect& rect, int padX, int padY) {
    return { rect.x - padX, rect.y - padY, rect.width + padX * 2, rect.height + padY * 2 };
}

Rect MakeEnergyPanelRect(const ScreenManager& screen) {
    return { 16, screen.GetHeight() - 16, 20, 6 };
}

void RenderFrameBox(ScreenManager& screen, const Rect& rect, WORD color) {
    screen.DrawString(rect.x, rect.y, "+" + string(rect.width - 2, '-') + "+", color);
    for (int row = 1; row < rect.height - 1; ++row) {
        screen.DrawString(rect.x, rect.y + row, "|" + string(rect.width - 2, ' ') + "|", color);
    }
    screen.DrawString(rect.x, rect.y + rect.height - 1, "+" + string(rect.width - 2, '-') + "+", color);
}

void RenderPileBox(ScreenManager& screen, const Rect& rect, const string& title, int count, bool highlighted) {
    const WORD color = highlighted ? COLOR_YELLOW : COLOR_WHITE;

    RenderFrameBox(screen, rect, color);

    screen.DrawString(rect.x + 2, rect.y + 1, title, color);
    screen.DrawString(rect.x + 2, rect.y + 2, "Count " + to_string(count), color);
}

WORD GetIntentColor(EnemyIntentType type) {
    switch (type) {
    case EnemyIntentType::Attack:
        return COLOR_RED;
    case EnemyIntentType::Defend:
        return COLOR_BLUE;
    case EnemyIntentType::Buff:
    default:
        return COLOR_YELLOW;
    }
}

string BuildIntentTitle(const EnemyIntentState& intent) {
    switch (intent.type) {
    case EnemyIntentType::Attack:
        return u8"공격 예고";
    case EnemyIntentType::Defend:
        return u8"방어 예고";
    case EnemyIntentType::Buff:
    default:
        return u8"강화 예고";
    }
}

string BuildIntentDetail(const EnemyIntentState& intent) {
    switch (intent.type) {
    case EnemyIntentType::Attack:
        return string(u8"피해 ") + to_string(intent.value);
    case EnemyIntentType::Defend:
        return string(u8"방어도 ") + to_string(intent.value);
    case EnemyIntentType::Buff:
    default:
        return string(u8"힘 +") + to_string(intent.value);
    }
}

string BuildIntentTag(const EnemyIntentState& intent) {
    switch (intent.type) {
    case EnemyIntentType::Attack:
        return "[ATK]";
    case EnemyIntentType::Defend:
        return "[DEF]";
    case EnemyIntentType::Buff:
    default:
        return "[BUF]";
    }
}

string BuildStatusText(const EntityData& entity) {
    vector<string> tokens;

    if (entity.block > 0) {
        tokens.push_back(string(u8"방어 ") + to_string(entity.block));
    }
    if (entity.strength > 0) {
        tokens.push_back(string(u8"힘 ") + to_string(entity.strength));
    }
    if (entity.vulnerable > 0) {
        tokens.push_back(string(u8"취약 ") + to_string(entity.vulnerable));
    }
    if (entity.weak > 0) {
        tokens.push_back(string(u8"약화 ") + to_string(entity.weak));
    }
    if (entity.poison > 0) {
        tokens.push_back(string(u8"독 ") + to_string(entity.poison));
    }

    if (tokens.empty()) {
        return u8"상태 없음";
    }

    string combined;
    for (size_t index = 0; index < tokens.size(); ++index) {
        if (index > 0) {
            combined += "  ";
        }
        combined += tokens[index];
    }

    return combined;
}

void RenderStatusLine(ScreenManager& screen, int x, int y, int width, const EntityData& entity, WORD color) {
    const wstring alignedText = TextLayout::AlignToWidth(
        TextLayout::Utf8ToWide(BuildStatusText(entity)),
        width,
        TextLayout::HorizontalAlign::Center);
    screen.DrawString(x, y, alignedText, color);
}

void RenderEnergyPanel(ScreenManager& screen, const Rect& rect, const CombatSystem& combat) {
    RenderFrameBox(screen, rect, COLOR_YELLOW);

    const wstring title = TextLayout::AlignToWidth(TextLayout::Utf8ToWide(u8"에너지"), rect.width - 2, TextLayout::HorizontalAlign::Center);
    screen.DrawString(rect.x + 1, rect.y + 1, title, COLOR_YELLOW);

    string energyBar(static_cast<size_t>(combat.GetMaxEnergy()), '.');
    for (int index = 0; index < combat.GetEnergy() && index < static_cast<int>(energyBar.size()); ++index) {
        energyBar[static_cast<size_t>(index)] = '#';
    }

    const string gaugeText = "[" + energyBar + "] " + to_string(combat.GetEnergy()) + "/" + to_string(combat.GetMaxEnergy());
    screen.DrawString(rect.x + 2, rect.y + 2, gaugeText, COLOR_WHITE);
    screen.DrawString(rect.x + 2, rect.y + 3, string(u8"속도 x") + FormatFloat(combat.GetSpeedMultiplier(), 2), COLOR_GREEN);
    screen.DrawString(rect.x + 2, rect.y + 4, string(u8"손패 ") + to_string(static_cast<int>(combat.GetHand().size())) + "/" + to_string(combat.GetHandLimit()), FOREGROUND_INTENSITY);
}

void RenderIntentPanel(ScreenManager& screen, int x, int y, int width, const CombatSystem& combat) {
    const Rect rect = { x, y, width, 5 };
    const EnemyIntentState& intent = combat.GetCurrentIntent();
    const WORD intentColor = GetIntentColor(intent.type);

    RenderFrameBox(screen, rect, intentColor);

    screen.DrawString(rect.x + 2, rect.y + 1, BuildIntentTag(intent), intentColor);
    screen.DrawString(rect.x + 8, rect.y + 1, BuildIntentTitle(intent), intentColor);
    screen.DrawString(rect.x + 2, rect.y + 2, BuildIntentDetail(intent), COLOR_WHITE);

    const int barWidth = width - 14;
    const int fillWidth = static_cast<int>(std::round(static_cast<float>(barWidth) * combat.GetEnemyIntentProgress01()));
    string progressBar(static_cast<size_t>(barWidth), '.');
    for (int index = 0; index < fillWidth && index < barWidth; ++index) {
        progressBar[static_cast<size_t>(index)] = '#';
    }

    screen.DrawString(rect.x + 2, rect.y + 3, "[" + progressBar + "]", intentColor);
    screen.DrawString(rect.x + width - 8, rect.y + 3, FormatFloat(combat.GetEnemyIntentRemainingSec(), 1) + "s", COLOR_WHITE);
}

} // namespace

int main() {
    SetConsoleOutputCP(CP_UTF8);

    ScreenManager screen;
    InputManager input;
    AudioManager audio;
    TargetingArrow arrow;

    EntityData playerData = {};
    EntityData enemyData = {};
    ResetPrototypeEntities(playerData, enemyData);

    long long playerGold = 99;
    int currentFloor = 1;

    const int centerX = screen.GetCenterX();
    const int centerY = screen.GetCenterY();
    const int playerBaseX = centerX - 40;
    const int playerBaseY = centerY - 10;
    const int enemyBaseX = centerX + 20;
    const int enemyBaseY = centerY - 10;

    HUDPanel hud(screen.GetWidth(), &playerData, &playerGold, &currentFloor);
    EntityUI playerUI(playerBaseX, playerBaseY, &playerData, true);
    EntityUI enemyUI(enemyBaseX, enemyBaseY, &enemyData, false);

    CombatSystem combat(&playerData, &enemyData);
    CombatConfig& combatConfig = combat.GetMutableConfig();
    combatConfig.baseDrawIntervalSec = 2.5f;
    combatConfig.minDrawIntervalSec = 1.3f;
    combatConfig.baseEnergyIntervalSec = 3.0f;
    combatConfig.minEnergyIntervalSec = 0.9f;
    combatConfig.baseEnemyIntentIntervalSec = 6.0f;
    combatConfig.minEnemyIntentIntervalSec = 2.0f;
    combatConfig.speedGainPerEnemyAction = 0.05f;
    combatConfig.dragSlowStrength = 0.04f;
    combatConfig.startingHandSize = 5;
    combatConfig.handLimit = 8;
    combatConfig.startingEnergy = 3;
    combatConfig.maxEnergy = 5;

    vector<CardUI> handCardUIs;
    vector<FloatingText> floatingTexts;
    float impactPauseRemainingSec = 0.0f;
    constexpr float kImpactPauseDurationSec = 0.035f;
    constexpr float kImpactPauseTimeScale = 0.15f;

    auto syncHandUI = [&]() {
        handCardUIs.clear();
        const vector<CardData>& hand = combat.GetHand();
        handCardUIs.reserve(hand.size());
        for (size_t index = 0; index < hand.size(); ++index) {
            handCardUIs.emplace_back(0, 0, const_cast<CardData*>(&hand[index]));
        }
        };

    auto startPrototypeBattle = [&]() {
        ResetPrototypeEntities(playerData, enemyData);
        combat.StartBattle(BuildPrototypeDeck());
        syncHandUI();
        floatingTexts.clear();
        arrow.SetActive(false);
        impactPauseRemainingSec = 0.0f;
        };

    MapRenderer mapRenderer(screen.GetWidth(), screen.GetHeight());
    mapRenderer.GenerateDummyMap();

    ButtonUI btnToggleView(5, screen.GetHeight() - 6, 16, 3, u8"지도 / 전투", COLOR_WHITE, COLOR_YELLOW);

    TooltipUI contextTooltip;

    ModalPopupUI victoryPopup(40, 13, u8"프로토타입 승리");
    victoryPopup.SetContents({
        u8"실시간 전투가 종료되었습니다.",
        u8"계속하기를 누르면 같은 전투를 다시 시작합니다."
        });
    ButtonUI btnVictoryContinue(screen.GetCenterX() - 8, screen.GetCenterY() + 2, 16, 3, u8"계속하기", COLOR_WHITE, COLOR_YELLOW);
    victoryPopup.AddButton(btnVictoryContinue);

    ModalPopupUI defeatPopup(40, 13, u8"프로토타입 패배");
    defeatPopup.SetContents({
        u8"플레이어의 체력이 0이 되었습니다.",
        u8"다시 시도하기를 누르면 전투를 재시작합니다."
        });
    ButtonUI btnRetry(screen.GetCenterX() - 8, screen.GetCenterY() + 2, 16, 3, u8"다시 시도", COLOR_WHITE, COLOR_YELLOW);
    defeatPopup.AddButton(btnRetry);

    const int campfireX = 10;
    const int campfireY = centerY;
    Animator campfireAnim(campfireX, campfireY, FX_CAMPFIRE, 150, AnimMode::LOOP, FOREGROUND_RED | FOREGROUND_INTENSITY);

    audio.PlayBGM(L"Exordium.wav");
    audio.PlayEffect(L"campfire.wav", L"fire_sfx", true);

    startPrototypeBattle();

    ViewState currentView = ViewState::Combat;
    int draggedCardIndex = -1;
    bool isRunning = true;
    bool mKeyPressedLastFrame = false;

    using Clock = std::chrono::steady_clock;
    const int targetRefreshRate = GetTargetRefreshRate();
    const auto targetFrameTime = std::chrono::microseconds(1000000 / targetRefreshRate);
    auto previousFrameTime = Clock::now();
    auto fpsSampleStartTime = previousFrameTime;
    int sampledFrames = 0;
    double displayedFps = 0.0;
    double displayedFrameMs = 0.0;

    while (isRunning) {
        const auto frameStartTime = Clock::now();
        displayedFrameMs = std::chrono::duration<double, std::milli>(frameStartTime - previousFrameTime).count();
        previousFrameTime = frameStartTime;
        ++sampledFrames;

        const double fpsSampleSeconds = std::chrono::duration<double>(frameStartTime - fpsSampleStartTime).count();
        if (fpsSampleSeconds >= 0.25) {
            displayedFps = sampledFrames / fpsSampleSeconds;
            sampledFrames = 0;
            fpsSampleStartTime = frameStartTime;
        }

        const float deltaTimeSec = static_cast<float>(displayedFrameMs * 0.001);
        if (impactPauseRemainingSec > 0.0f) {
            impactPauseRemainingSec = (std::max)(0.0f, impactPauseRemainingSec - deltaTimeSec);
        }

        input.Update();

        const int mouseX = input.GetMouseX();
        const int mouseY = input.GetMouseY();
        const Rect drawPileRect = MakeDrawPileRect(screen);
        const Rect discardPileRect = MakeDiscardPileRect(screen);
        const Rect energyPanelRect = MakeEnergyPanelRect(screen);
        const Rect discardDropRect = ExpandRect(discardPileRect, 6, 3);

        if (input.IsEscPressed()) {
            if (currentView == ViewState::Map) {
                currentView = ViewState::Combat;
            }
            else {
                isRunning = false;
            }
        }

        const bool mKeyPressed = (GetAsyncKeyState('M') & 0x8000) != 0;
        if (mKeyPressed && !mKeyPressedLastFrame) {
            currentView = (currentView == ViewState::Combat) ? ViewState::Map : ViewState::Combat;
        }
        mKeyPressedLastFrame = mKeyPressed;

        const int sourceX = campfireX + 2;
        const int sourceY = campfireY + 2;
        const int maxDist = 240;
        audio.UpdateSpatialVolume(sourceX, sourceY, mouseX, mouseY, L"fire_sfx", maxDist);

        const double dx = static_cast<double>(mouseX - sourceX);
        const double dy = static_cast<double>(mouseY - sourceY) * 2.0;
        const double dbgDistance = std::sqrt(dx * dx + dy * dy);
        const double dbgNormDist = (dbgDistance > maxDist) ? 1.0 : (dbgDistance / maxDist);
        const double dbgVolRatio = std::pow(1.0 - dbgNormDist, 2.0);
        int dbgVolume = static_cast<int>(1000 * dbgVolRatio);
        if (dbgVolume < 0) dbgVolume = 0;

        screen.Clear();

        if (currentView == ViewState::Combat) {
            const bool popupVisible = victoryPopup.IsVisible() || defeatPopup.IsVisible();

            if (!popupVisible) {
                float combatTimeScale = (draggedCardIndex != -1) ? combat.GetDragTimeScale() : 1.0f;
                if (impactPauseRemainingSec > 0.0f) {
                    combatTimeScale *= kImpactPauseTimeScale;
                }
                const CombatFrameResult frameResult = combat.Update(deltaTimeSec, combatTimeScale);

                if (frameResult.handChanged) {
                    syncHandUI();
                }

                if (frameResult.overdrawRejected) {
                    floatingTexts.emplace_back(drawPileRect.x + 2, drawPileRect.y - 1, "FULL", COLOR_YELLOW);
                }

                if (frameResult.playerHit) {
                    impactPauseRemainingSec = kImpactPauseDurationSec;
                    playerUI.TriggerHitAnimation();
                    if (frameResult.damageToPlayer > 0) {
                        floatingTexts.emplace_back(playerBaseX + 8, playerBaseY - 1, "-" + to_string(frameResult.damageToPlayer), COLOR_RED);
                    }
                    else {
                        floatingTexts.emplace_back(playerBaseX + 8, playerBaseY - 1, "BLOCK", COLOR_BLUE);
                    }
                }

                if (frameResult.enemyHit && frameResult.damageToEnemy > 0) {
                    impactPauseRemainingSec = kImpactPauseDurationSec;
                    enemyUI.TriggerHitAnimation();
                    floatingTexts.emplace_back(enemyBaseX + 8, enemyBaseY - 1, "-" + to_string(frameResult.damageToEnemy), COLOR_RED);
                }

                if (frameResult.enemyBlockGained > 0) {
                    floatingTexts.emplace_back(enemyBaseX + 7, enemyBaseY - 2, "+" + to_string(frameResult.enemyBlockGained) + " Block", COLOR_BLUE);
                }

                if (frameResult.enemyStrengthGained > 0) {
                    floatingTexts.emplace_back(enemyBaseX + 7, enemyBaseY - 3, "STR +" + to_string(frameResult.enemyStrengthGained), COLOR_YELLOW);
                }

                if (combat.IsBattleOver()) {
                    if (combat.DidPlayerWin()) {
                        victoryPopup.Open();
                    }
                    else {
                        defeatPopup.Open();
                    }
                }
            }

            if (victoryPopup.IsVisible()) {
                victoryPopup.CenterInScreen(screen.GetWidth(), screen.GetHeight());
                victoryPopup.Update(input);
                if (victoryPopup.IsButtonClicked(0)) {
                    victoryPopup.Close();
                    startPrototypeBattle();
                    draggedCardIndex = -1;
                }
            }
            else if (defeatPopup.IsVisible()) {
                defeatPopup.CenterInScreen(screen.GetWidth(), screen.GetHeight());
                defeatPopup.Update(input);
                if (defeatPopup.IsButtonClicked(0)) {
                    defeatPopup.Close();
                    startPrototypeBattle();
                    draggedCardIndex = -1;
                }
            }
            else {
                playerUI.Update(input);
                enemyUI.Update(input);
                hud.Update(input);
                btnToggleView.Update(input);
                campfireAnim.Update();

                if (btnToggleView.IsClicked()) {
                    currentView = ViewState::Map;
                }

                const int numCards = static_cast<int>(handCardUIs.size());
                const int cardWidth = 28;
                const int overlapChars = (numCards <= 5) ? 2 : ((numCards <= 7) ? 3 : 4);
                const int spacing = cardWidth - overlapChars;
                const int totalHandWidth = (numCards > 0) ? cardWidth + (numCards - 1) * spacing : 0;
                const int startX = (screen.GetWidth() - totalHandWidth) / 2;
                const int startY = screen.GetHeight() - 18 - 2;

                for (int cardIndex = 0; cardIndex < numCards; ++cardIndex) {
                    handCardUIs[cardIndex].SetBasePosition(startX + (cardIndex * spacing), startY);
                }

                int currentHoveredIndex = -1;
                bool anyCardHovered = false;
                for (int cardIndex = numCards - 1; cardIndex >= 0; --cardIndex) {
                    if (!anyCardHovered && handCardUIs[cardIndex].Update(input)) {
                        anyCardHovered = true;
                        currentHoveredIndex = cardIndex;
                    }
                    else {
                        handCardUIs[cardIndex].SetHovered(false);
                        handCardUIs[cardIndex].Update(input);
                        handCardUIs[cardIndex].SetHovered(false);
                    }
                }

                CombatDropTarget dropTarget = CombatDropTarget::None;
                if (draggedCardIndex != -1 && draggedCardIndex < static_cast<int>(combat.GetHand().size())) {
                    const CardData& draggedCard = combat.GetHand()[static_cast<size_t>(draggedCardIndex)];

                    if (discardDropRect.Contains(mouseX, mouseY)) {
                        dropTarget = CombatDropTarget::DiscardPile;
                    }
                    else if (draggedCard.targetType == CardTargetType::Enemy && enemyUI.IsPointInside(mouseX, mouseY)) {
                        dropTarget = CombatDropTarget::Enemy;
                    }
                    else if (draggedCard.targetType == CardTargetType::Self && playerUI.IsPointInside(mouseX, mouseY)) {
                        dropTarget = CombatDropTarget::Player;
                    }

                    enemyUI.SetTargeted(dropTarget == CombatDropTarget::Enemy);
                    playerUI.SetTargeted(dropTarget == CombatDropTarget::Player);
                }

                if (input.IsLeftClickDown() && draggedCardIndex == -1 && currentHoveredIndex != -1) {
                    draggedCardIndex = currentHoveredIndex;
                    arrow.SetActive(true);
                }

                if (input.IsLeftClick() && draggedCardIndex != -1 && draggedCardIndex < static_cast<int>(handCardUIs.size())) {
                    const int cardStartX = handCardUIs[draggedCardIndex].GetX() + (handCardUIs[draggedCardIndex].GetWidth() / 2);
                    const int cardStartY = handCardUIs[draggedCardIndex].GetY();
                    arrow.SetStartPoint(cardStartX, cardStartY);
                    arrow.SetEndPoint(mouseX, mouseY);
                }

                if (input.IsLeftClickUp() && draggedCardIndex != -1) {
                    CombatActionResult actionResult;

                    if (dropTarget == CombatDropTarget::DiscardPile) {
                        actionResult = combat.TryDiscardCard(draggedCardIndex);
                    }
                    else if (dropTarget != CombatDropTarget::None) {
                        actionResult = combat.TryUseCard(draggedCardIndex, dropTarget);
                    }

                    if (actionResult.success) {
                        if (actionResult.handChanged) {
                            syncHandUI();
                        }

                        if (actionResult.damageToEnemy > 0) {
                            impactPauseRemainingSec = kImpactPauseDurationSec;
                            enemyUI.TriggerHitAnimation();
                            floatingTexts.emplace_back(enemyBaseX + 8, enemyBaseY - 2, "-" + to_string(actionResult.damageToEnemy), COLOR_RED);
                        }

                        if (actionResult.blockGained > 0) {
                            floatingTexts.emplace_back(playerBaseX + 4, playerBaseY - 2, "+" + to_string(actionResult.blockGained) + " Block", COLOR_GREEN);
                        }

                        if (actionResult.vulnerableApplied > 0) {
                            floatingTexts.emplace_back(enemyBaseX + 4, enemyBaseY - 3, "VULN +" + to_string(actionResult.vulnerableApplied), COLOR_YELLOW);
                        }

                        if (actionResult.energyGained > 0) {
                            floatingTexts.emplace_back(energyPanelRect.x + 4, energyPanelRect.y - 1, "+E " + to_string(actionResult.energyGained), COLOR_YELLOW);
                        }

                        if (actionResult.cardsDrawn > 0) {
                            floatingTexts.emplace_back(drawPileRect.x + 1, drawPileRect.y - 3, "+Draw " + to_string(actionResult.cardsDrawn), COLOR_GREEN);
                        }

                        if (combat.IsBattleOver()) {
                            if (combat.DidPlayerWin()) {
                                victoryPopup.Open();
                            }
                            else {
                                defeatPopup.Open();
                            }
                        }
                    }
                    else if (!actionResult.message.empty()) {
                        const string message = (actionResult.message == "Not enough energy") ? "NO EN" : "BAD";
                        floatingTexts.emplace_back(mouseX, mouseY, message, COLOR_YELLOW);
                    }

                    enemyUI.SetTargeted(false);
                    playerUI.SetTargeted(false);
                    arrow.SetActive(false);
                    draggedCardIndex = -1;
                }

                if (mouseX >= 2 && mouseX <= 9 && mouseY == 6) {
                    contextTooltip.SetText({
                        u8"불타는 피",
                        u8"전투 종료 시, 체력을 6 회복합니다."
                        });
                    contextTooltip.SetVisible(true);
                    contextTooltip.UpdatePosition(mouseX, mouseY, screen.GetWidth(), screen.GetHeight());
                }
                else {
                    contextTooltip.SetVisible(false);
                }
            }

            for (auto it = floatingTexts.begin(); it != floatingTexts.end();) {
                if (!it->Update()) {
                    it = floatingTexts.erase(it);
                }
                else {
                    ++it;
                }
            }

            screen.DrawString(2, 6, u8"[버프]", COLOR_RED);
            RenderIntentPanel(screen, enemyBaseX - 2, enemyBaseY - 7, 26, combat);
            RenderStatusLine(screen, playerBaseX, playerBaseY + 8, 24, playerData, COLOR_GREEN);
            RenderStatusLine(screen, enemyBaseX, enemyBaseY + 8, 24, enemyData, COLOR_YELLOW);
            RenderEnergyPanel(screen, energyPanelRect, combat);

            campfireAnim.Render(screen);
            RenderPileBox(screen, drawPileRect, u8"뽑기", combat.GetDrawPileCount(), false);
            RenderPileBox(screen, discardPileRect, u8"버리기", combat.GetDiscardPileCount(), discardDropRect.Contains(mouseX, mouseY));
            btnToggleView.Render(screen);
            playerUI.Render(screen);
            enemyUI.Render(screen);

            const int topRenderIndex = draggedCardIndex;
            for (int cardIndex = 0; cardIndex < static_cast<int>(handCardUIs.size()); ++cardIndex) {
                if (cardIndex != topRenderIndex) {
                    handCardUIs[cardIndex].Render(screen);
                }
            }
            if (topRenderIndex != -1 && topRenderIndex < static_cast<int>(handCardUIs.size())) {
                handCardUIs[topRenderIndex].Render(screen);
            }

            arrow.Render(screen);
            for (auto& text : floatingTexts) {
                text.Render(screen);
            }

            contextTooltip.Render(screen);
            victoryPopup.Render(screen);
            defeatPopup.Render(screen);
        }
        else {
            btnToggleView.Update(input);
            if (btnToggleView.IsClicked()) {
                currentView = ViewState::Combat;
            }

            mapRenderer.Update(input);
            mapRenderer.Render(screen);
            btnToggleView.Render(screen);

            vector<string> mapTooltipLines;
            if (mapRenderer.TryGetNodeTooltip(mouseX, mouseY, mapTooltipLines)) {
                contextTooltip.SetText(mapTooltipLines);
                contextTooltip.SetVisible(true);
                contextTooltip.UpdatePosition(mouseX, mouseY, screen.GetWidth(), screen.GetHeight());
            }
            else {
                contextTooltip.SetVisible(false);
            }

            const string mapTitle = u8"[ 지도 보기 ]";
            const int mapTitleX = TextLayout::ComputeAlignedXUtf8(0, screen.GetWidth(), mapTitle, TextLayout::HorizontalAlign::Center);
            screen.DrawString(mapTitleX, 2, mapTitle, COLOR_YELLOW);
            contextTooltip.Render(screen);
        }

        hud.Render(screen);

        const int debugBaseY = screen.GetHeight() - 54;
        const string dbgDistanceText = string(u8"거리: ") + to_string(static_cast<int>(dbgDistance));
        const string dbgVolumeText = string(u8"볼륨: ") + to_string(dbgVolume);
        const string dbgMouse = string(u8"마우스 (X:") + to_string(mouseX) + ", Y:" + to_string(mouseY) + ")";

        string dbgState = u8"좌클릭: ";
        if (input.IsLeftClickDown()) dbgState += u8"[방금 누름]";
        else if (input.IsLeftClickUp()) dbgState += u8"[방금 뗌]";
        else if (input.IsLeftClick()) dbgState += u8"[누르는 중]";
        else dbgState += u8"[대기]";

        const string dbgWheel = string(u8"휠: ") + to_string(input.GetWheelDelta());
        const string dbgFps = "FPS: " + to_string(static_cast<int>(std::round(displayedFps)));
        const string dbgFrame = "Frame: " + to_string(static_cast<int>(std::round(displayedFrameMs))) + " ms";
        const string dbgTarget = "Target: " + to_string(targetRefreshRate) + " Hz";

        screen.DrawString(2, debugBaseY + 0, dbgDistanceText, COLOR_WHITE);
        screen.DrawString(2, debugBaseY + 1, dbgVolumeText, COLOR_YELLOW);
        screen.DrawString(2, debugBaseY + 2, dbgMouse, COLOR_WHITE);
        screen.DrawString(30, debugBaseY + 2, dbgState, input.IsLeftClick() ? COLOR_RED : COLOR_WHITE);
        screen.DrawString(30, debugBaseY + 3, dbgWheel, COLOR_WHITE);
        screen.DrawString(2, debugBaseY + 4, dbgFps, COLOR_GREEN);
        screen.DrawString(16, debugBaseY + 4, dbgFrame, COLOR_GREEN);
        screen.DrawString(34, debugBaseY + 4, dbgTarget, COLOR_GREEN);

        screen.DrawChar(mouseX, mouseY, '+', COLOR_GREEN);
        screen.Render();

        const auto frameEndTime = Clock::now();
        const auto frameWorkTime = frameEndTime - frameStartTime;
        if (frameWorkTime < targetFrameTime) {
            const auto remainingTime = targetFrameTime - frameWorkTime;
            const auto sleepTime = std::chrono::duration_cast<std::chrono::milliseconds>(remainingTime);

            if (sleepTime.count() > 1) {
                Sleep(static_cast<DWORD>(sleepTime.count() - 1));
            }

            while ((Clock::now() - frameStartTime) < targetFrameTime) {
                Sleep(0);
            }
        }
    }

    audio.StopEffect(L"fire_sfx");
    return 0;
}
