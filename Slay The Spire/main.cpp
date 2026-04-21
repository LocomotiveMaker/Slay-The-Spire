// -----------------------------------------------------------------------------
// @file       main.cpp
// @brief      Slay The Spire console tech demo entry point.
// -----------------------------------------------------------------------------
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "Animation.h"
#include "AudioManager.h"
#include "ButtonUI.h"
#include "CardUI.h"
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
enum class TargetType { None, Monster, Player, Card };

const vector<vector<string>> FX_CAMPFIRE = {
    { "  (  ", " ) ( ", "( * )", " /|\\ ", "[===]" },
    { " (   ", "  )  ", "( * )", " /|\\ ", "[===]" },
    { "   ) ", " ( ( ", "( * )", " /|\\ ", "[===]" }
};

int main() {
    SetConsoleOutputCP(CP_UTF8);

    ScreenManager screen;
    InputManager input;
    AudioManager audio;
    TargetingArrow arrow;

    EntityData playerData = { 0, u8"아이언클래드", 80, 80, 5 };
    EntityData monsterData = { 1, u8"조 웜", 12, 44, 0 };
    long long playerGold = 99;
    int currentFloor = 1;

    vector<CardData> handData = {
        { 0, u8"강타", 1, u8"적에게 6 피해를 줍니다.", CardType::Attack },
        { 1, u8"수비", 1, u8"방어도 5를 얻습니다.", CardType::Skill },
        { 2, u8"전투 함성", 1, u8"카드 1장을 뽑고, 에너지 1을 얻습니다.", CardType::Skill },
        { 3, u8"출혈 베기", 1, u8"적에게 4 피해를 두 번 줍니다.", CardType::Attack },
        { 4, u8"불굴", 2, u8"이번 턴 동안, 받는 피해를 2 감소시킵니다.", CardType::Power }
    };

    const int centerX = screen.GetCenterX();
    const int centerY = screen.GetCenterY();

    HUDPanel hud(screen.GetWidth(), &playerData, &playerGold, &currentFloor);
    EntityUI playerUI(centerX - 40, centerY - 10, &playerData, true);
    EntityUI monsterUI(centerX + 20, centerY - 10, &monsterData, false);

    vector<CardUI> handCardUIs;
    handCardUIs.reserve(handData.size());
    for (size_t i = 0; i < handData.size(); ++i) {
        handCardUIs.emplace_back(0, 0, &handData[i]);
    }

    MapRenderer mapRenderer(screen.GetWidth(), screen.GetHeight());
    mapRenderer.GenerateDummyMap();

    ButtonUI btnToggleView(5, screen.GetHeight() - 6, 16, 3, u8"지도 / 전투", COLOR_WHITE, COLOR_YELLOW);

    TooltipUI contextTooltip;
    ModalPopupUI victoryPopup(40, 15, u8"전투 승리!");
    victoryPopup.SetContents({
        u8"몬스터를 처치했습니다.",
        u8"보상을 선택하고, 다음 노드로 이동하세요."
    });

    ButtonUI btnContinue(screen.GetCenterX() - 8, screen.GetCenterY() + 3, 16, 3, u8"계속하기", COLOR_WHITE, COLOR_YELLOW);
    victoryPopup.AddButton(btnContinue);

    const int campfireX = 10;
    const int campfireY = centerY;
    Animator campfireAnim(campfireX, campfireY, FX_CAMPFIRE, 150, AnimMode::LOOP, FOREGROUND_RED | FOREGROUND_INTENSITY);

    audio.PlayBGM(L"Exordium.wav");
    audio.PlayEffect(L"campfire.wav", L"fire_sfx", true);

    ViewState currentView = ViewState::Combat;
    vector<FloatingText> floatingTexts;
    int draggedCardIndex = -1;
    bool isRunning = true;
    bool mKeyPressedLastFrame = false;

    while (isRunning) {
        int currentHoveredIndex = -1;
        bool shouldShowTooltip = false;

        input.Update();

        const int mouseX = input.GetMouseX();
        const int mouseY = input.GetMouseY();

        if (input.IsEscPressed()) {
            if (currentView == ViewState::Map) {
                currentView = ViewState::Combat;
            }
            else {
                isRunning = false;
            }
        }

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

        const bool mKeyPressed = (GetAsyncKeyState('M') & 0x8000) != 0;
        if (mKeyPressed && !mKeyPressedLastFrame) {
            currentView = (currentView == ViewState::Combat) ? ViewState::Map : ViewState::Combat;
        }
        mKeyPressedLastFrame = mKeyPressed;

        screen.Clear();

        if (currentView == ViewState::Combat) {
            if (victoryPopup.IsVisible()) {
                victoryPopup.CenterInScreen(screen.GetWidth(), screen.GetHeight());
                victoryPopup.Update(input);
                if (victoryPopup.IsButtonClicked(0)) {
                    victoryPopup.Close();
                    currentView = ViewState::Map;
                }
            }
            else {
                playerUI.Update(input);
                monsterUI.Update(input);
                hud.Update(input);
                btnToggleView.Update(input);
                campfireAnim.Update();

                if (btnToggleView.IsClicked()) {
                    currentView = ViewState::Map;
                    audio.PlayEffect(L"card_pick_sfx.wav", L"ui_click");
                }

                for (auto it = floatingTexts.begin(); it != floatingTexts.end();) {
                    if (!it->Update()) it = floatingTexts.erase(it);
                    else ++it;
                }

                const int numCards = static_cast<int>(handCardUIs.size());
                const int cardWidth = 28;
                const int overlapChars = (numCards <= 5) ? 2 : ((numCards <= 7) ? 3 : ((numCards <= 9) ? 4 : 5));
                const int spacing = cardWidth - overlapChars;
                const int totalHandWidth = (numCards > 0) ? cardWidth + (numCards - 1) * spacing : 0;
                const int startX = (screen.GetWidth() - totalHandWidth) / 2;
                const int startY = screen.GetHeight() - 18 - 2;

                for (int i = 0; i < numCards; ++i) {
                    handCardUIs[i].SetBasePosition(startX + (i * spacing), startY);
                }

                bool isAnyCardHovered = false;
                for (int i = numCards - 1; i >= 0; --i) {
                    if (!isAnyCardHovered && handCardUIs[i].Update(input)) {
                        isAnyCardHovered = true;
                        currentHoveredIndex = i;
                    }
                    else {
                        handCardUIs[i].SetHovered(false);
                        handCardUIs[i].Update(input);
                        handCardUIs[i].SetHovered(false);
                    }
                }

                TargetType currentTarget = TargetType::None;
                if (draggedCardIndex != -1) {
                    if (monsterUI.IsPointInside(mouseX, mouseY)) currentTarget = TargetType::Monster;
                    else if (playerUI.IsPointInside(mouseX, mouseY)) currentTarget = TargetType::Player;
                    else {
                        for (int i = 0; i < static_cast<int>(handCardUIs.size()); ++i) {
                            if (i != draggedCardIndex && handCardUIs[i].IsPointInside(mouseX, mouseY)) {
                                currentTarget = TargetType::Card;
                                break;
                            }
                        }
                    }
                    monsterUI.SetTargeted(currentTarget == TargetType::Monster);
                    playerUI.SetTargeted(currentTarget == TargetType::Player);
                }

                if (input.IsLeftClickDown() && draggedCardIndex == -1 && currentHoveredIndex != -1) {
                    draggedCardIndex = currentHoveredIndex;
                    arrow.SetActive(true);
                    audio.PlayEffect(L"card_pick_sfx.wav", L"card_drag");
                }

                if (input.IsLeftClick() && draggedCardIndex != -1) {
                    const int cardStartX = handCardUIs[draggedCardIndex].GetX() + (handCardUIs[draggedCardIndex].GetWidth() / 2);
                    const int cardStartY = handCardUIs[draggedCardIndex].GetY();
                    arrow.SetStartPoint(cardStartX, cardStartY);
                    arrow.SetEndPoint(mouseX, mouseY);
                }

                if (input.IsLeftClickUp() && draggedCardIndex != -1) {
                    if (currentTarget == TargetType::Monster) {
                        monsterData.currentHp -= 6;
                        if (monsterData.currentHp <= 0) {
                            monsterData.currentHp = 0;
                            victoryPopup.Open();
                        }
                        monsterUI.TriggerHitAnimation();
                        floatingTexts.emplace_back(mouseX, mouseY - 2, "-6", COLOR_RED);
                        audio.PlayEffect(L"chip_effect_sfx.wav", L"hit_sfx");
                        handCardUIs.erase(handCardUIs.begin() + draggedCardIndex);
                    }
                    monsterUI.SetTargeted(false);
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
                    shouldShowTooltip = true;
                }
            }

            screen.DrawString(2, 6, u8"[버프]", COLOR_RED);

            campfireAnim.Render(screen);
            btnToggleView.Render(screen);
            playerUI.Render(screen);
            monsterUI.Render(screen);

            const int topRenderIndex = (draggedCardIndex != -1) ? draggedCardIndex : (victoryPopup.IsVisible() ? -1 : currentHoveredIndex);
            for (int i = 0; i < static_cast<int>(handCardUIs.size()); ++i) {
                if (i != topRenderIndex) handCardUIs[i].Render(screen);
            }
            if (topRenderIndex != -1 && topRenderIndex < static_cast<int>(handCardUIs.size())) {
                handCardUIs[topRenderIndex].Render(screen);
            }

            arrow.Render(screen);
            for (auto& text : floatingTexts) {
                text.Render(screen);
            }
            victoryPopup.Render(screen);
        }
        else if (currentView == ViewState::Map) {
            btnToggleView.Update(input);
            if (btnToggleView.IsClicked()) currentView = ViewState::Combat;

            mapRenderer.Update(input);
            mapRenderer.Render(screen);
            btnToggleView.Render(screen);

            vector<string> mapTooltipLines;
            if (mapRenderer.TryGetNodeTooltip(mouseX, mouseY, mapTooltipLines)) {
                contextTooltip.SetText(mapTooltipLines);
                contextTooltip.SetVisible(true);
                contextTooltip.UpdatePosition(mouseX, mouseY, screen.GetWidth(), screen.GetHeight());
                shouldShowTooltip = true;
            }

            const string mapTitle = u8"[ 지도 보기 ]";
            const int mapTitleX = TextLayout::ComputeAlignedXUtf8(0, screen.GetWidth(), mapTitle, TextLayout::HorizontalAlign::Center);
            screen.DrawString(mapTitleX, 2, mapTitle, COLOR_YELLOW);
        }

        if (!shouldShowTooltip) {
            contextTooltip.SetVisible(false);
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

        screen.DrawString(2, debugBaseY + 0, dbgDistanceText, COLOR_WHITE);
        screen.DrawString(2, debugBaseY + 1, dbgVolumeText, COLOR_YELLOW);
        screen.DrawString(2, debugBaseY + 2, dbgMouse, COLOR_WHITE);
        screen.DrawString(30, debugBaseY + 2, dbgState, input.IsLeftClick() ? COLOR_RED : COLOR_WHITE);
        screen.DrawString(30, debugBaseY + 3, dbgWheel, COLOR_WHITE);

        contextTooltip.Render(screen);
        screen.DrawChar(mouseX, mouseY, '+', COLOR_GREEN);
        screen.Render();
        Sleep(16);
    }

    audio.StopEffect(L"fire_sfx");
    return 0;
}
