// -----------------------------------------------------------------------------
// @file       main.cpp
// @brief      슬레이 더 스파이어 콘솔 클론 메인 실행 파일 (통합 시연 버전)
// -----------------------------------------------------------------------------
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

#include "ScreenManager.h"
#include "InputManager.h"
#include "AudioManager.h"
#include "CardUI.h"
#include "TargetingArrow.h"
#include "EntityUI.h"
#include "FloatingText.h"
#include "MapRenderer.h"
#include "ButtonUI.h"
#include "TooltipUI.h"
#include "ModalPopupUI.h"
#include "GameData.h"
#include "Animation.h"
#include "HUDPanel.h"

using namespace std;

enum class ViewState { Combat, Map };
enum class TargetType { None, Monster, Player, Card };

// 시연용 모닥불 프레임
const vector<vector<string>> FX_CAMPFIRE = {
    { "  (  ", " ) ( ", "( * )", " /|\\ ", "[===]" },
    { " (   ", "  )  ", "( * )", " /|\\ ", "[===]" },
    { "   ) ", " ( ( ", "( * )", " /|\\ ", "[===]" }
};

int main() {
    SetConsoleOutputCP(CP_UTF8);

    // 코어 시스템 초기화
    ScreenManager screen;
    InputManager input;
    AudioManager audio;
    TargetingArrow arrow;

    // =============================================================
    // 더미 데이터(Model) 생성
    // =============================================================
    EntityData playerData = { 0, "Ironclad", 80, 80, 5 };
    EntityData monsterData = { 1, "Jaw Worm", 12, 44, 0 }; // 2대 맞으면 죽도록 HP 설정
    long long playerGold = 99;
    int currentFloor = 1;

    vector<CardData> handData;
    for (int i = 0; i < 5; ++i) {
        handData.push_back({ i, "Strike" + to_string(i+1), 1, "Deal 6 dmg.", CardType::Attack });
    }

    // =============================================================
    // UI 객체(View)에 데이터 주입
    // =============================================================
    int centerX = screen.GetCenterX();
    int centerY = screen.GetCenterY();

    HUDPanel hud(screen.GetWidth(), &playerData, &playerGold, &currentFloor);

    EntityUI playerUI(centerX - 40, centerY - 10, &playerData, true);
    EntityUI monsterUI(centerX + 20, centerY - 10, &monsterData, false);

    vector<CardUI> handCardUIs;
    for (size_t i = 0; i < handData.size(); ++i) {
        handCardUIs.push_back(CardUI(0, 0, &handData[i]));
    }

    MapRenderer mapRenderer(screen.GetWidth(), screen.GetHeight());
    mapRenderer.GenerateDummyMap();

    ButtonUI btnToggleView(5, screen.GetHeight() - 6, 16, 3, "Map / Combat", COLOR_WHITE, COLOR_YELLOW);

    TooltipUI sampleTooltip;
    sampleTooltip.SetText({ "Burning Blood", "At the end of combat,", "heal 6 HP." });

    ModalPopupUI victoryPopup(40, 15, "VICTORY!");
    victoryPopup.SetContents({ "The monster is dead.", "", "Tech Demo Completed." });
    ButtonUI btnContinue(screen.GetCenterX() - 8, screen.GetCenterY() + 3, 16, 3, "Continue", COLOR_WHITE, COLOR_YELLOW);
    victoryPopup.AddButton(btnContinue);

    int campfireX = 10;
    int campfireY = centerY;
    Animator campfireAnim(campfireX, campfireY, FX_CAMPFIRE, 150, AnimMode::LOOP, FOREGROUND_RED | FOREGROUND_INTENSITY);

    audio.PlayBGM(L"Exordium.wav");

    audio.PlayEffect(L"campfire.wav", L"fire_sfx", true);

    // =============================================================
    // 메인 루프 변수
    // =============================================================
    ViewState currentView = ViewState::Combat;
    vector<FloatingText> floatingTexts;
    int draggedCardIndex = -1;
    bool isRunning = true;
    bool mKeyPressedLastFrame = false;

    // 메인 게임 루프
    while (isRunning) {

        int currentHoveredIndex = -1;

        // 1. 입력 업데이트
        input.Update();

        int mouseX = input.GetMouseX();
        int mouseY = input.GetMouseY();

        // 2. 글로벌 입력 처리 (종료, 뷰 전환)
        if (input.IsEscPressed()) {
            if (currentView == ViewState::Map) {
                currentView = ViewState::Combat;
            }
            else {
                isRunning = false;
            }
        }

        // -------------------------------------------------------------
        // 공간 음향 및 디버그 데이터 계산
        // -------------------------------------------------------------
        int sourceX = campfireX + 2;
        int sourceY = campfireY + 2;
        int maxDist = 240;

        audio.UpdateSpatialVolume(sourceX, sourceY, mouseX, mouseY, L"fire_sfx", maxDist);

        // 디버그용 데이터 계산 (AudioManager와 동일한 로직 사용)
        double dx = static_cast<double>(mouseX - sourceX);
        double dy = static_cast<double>(mouseY - sourceY) * 2.0;
        double dbgDistance = std::sqrt(dx * dx + dy * dy);
        double dbgNormDist = (dbgDistance > maxDist) ? 1.0 : (dbgDistance / maxDist);
        double dbgVolRatio = std::pow(1.0 - dbgNormDist, 2.0);
        int dbgVolume = static_cast<int>(1000 * dbgVolRatio);
        if (dbgVolume < 0) dbgVolume = 0;

        bool mKeyPressed = (GetAsyncKeyState('M') & 0x8000) != 0;
        if (mKeyPressed && !mKeyPressedLastFrame) {
            currentView = (currentView == ViewState::Combat) ? ViewState::Map : ViewState::Combat;
        }
        mKeyPressedLastFrame = mKeyPressed;

        // 3. 상태별 로직 업데이트
        screen.Clear();

        if (currentView == ViewState::Combat) {
            // [전투 화면 업데이트]
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

                for (auto it = floatingTexts.begin(); it != floatingTexts.end(); ) {
                    if (!it->Update()) it = floatingTexts.erase(it);
                    else ++it;
                }

                int numCards = handCardUIs.size();
                int cardWidth = 28;
                int overlapChars = (numCards <= 5) ? 2 : ((numCards <= 7) ? 3 : ((numCards <= 9) ? 4 : 5));
                int spacing = cardWidth - overlapChars;
                int totalHandWidth = (numCards > 0) ? cardWidth + (numCards - 1) * spacing : 0;
                int startX = (screen.GetWidth() - totalHandWidth) / 2;
                int startY = screen.GetHeight() - 18 - 2;

                for (int i = 0; i < numCards; ++i) {
                    handCardUIs[i].SetBasePosition(startX + (i * spacing), startY);
                }

                bool isAnyCardHovered = false;
                int currentHoveredIndex = -1;
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

                // 타겟 식별
                TargetType currentTarget = TargetType::None;
                if (draggedCardIndex != -1) {
                    if (monsterUI.IsPointInside(mouseX, mouseY)) currentTarget = TargetType::Monster;
                    else if (playerUI.IsPointInside(mouseX, mouseY)) currentTarget = TargetType::Player;
                    else {
                        for (int i = 0; i < handCardUIs.size(); ++i) {
                            if (i != draggedCardIndex && handCardUIs[i].IsPointInside(mouseX, mouseY)) {
                                currentTarget = TargetType::Card;
                                break;
                            }
                        }
                    }
                    monsterUI.SetTargeted(currentTarget == TargetType::Monster);
                    playerUI.SetTargeted(currentTarget == TargetType::Player);
                }

                // 드래그 앤 드롭
                if (input.IsLeftClickDown() && draggedCardIndex == -1 && currentHoveredIndex != -1) {
                    draggedCardIndex = currentHoveredIndex;
                    arrow.SetActive(true);
                    audio.PlayEffect(L"card_pick_sfx.wav", L"card_drag");
                }

                if (input.IsLeftClick() && draggedCardIndex != -1) {
                    int cardStartX = handCardUIs[draggedCardIndex].GetX() + (handCardUIs[draggedCardIndex].GetWidth() / 2);
                    int cardStartY = handCardUIs[draggedCardIndex].GetY();
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

                // 툴팁
                if (mouseX >= 2 && mouseX <= 8 && mouseY == 6) {
                    sampleTooltip.SetVisible(true);
                    sampleTooltip.UpdatePosition(mouseX, mouseY, screen.GetWidth(), screen.GetHeight());
                }
                else {
                    sampleTooltip.SetVisible(false);
                }
            }

            // [전투 화면 렌더링]
            screen.DrawString(2, 6, "[BUFF]", COLOR_RED);

            campfireAnim.Render(screen);
            btnToggleView.Render(screen);
            playerUI.Render(screen);
            monsterUI.Render(screen);

            int topRenderIndex = (draggedCardIndex != -1) ? draggedCardIndex : (victoryPopup.IsVisible() ? -1 : currentHoveredIndex);
            for (int i = 0; i < handCardUIs.size(); ++i) {
                if (i != topRenderIndex) handCardUIs[i].Render(screen);
            }
            if (topRenderIndex != -1 && topRenderIndex < handCardUIs.size()) {
                handCardUIs[topRenderIndex].Render(screen);
            }

            arrow.Render(screen);
            for (auto& text : floatingTexts) text.Render(screen);
            sampleTooltip.Render(screen);
            victoryPopup.Render(screen);
        }
        else if (currentView == ViewState::Map) {
            // [지도 화면 처리]
            mapRenderer.Update(input);
            btnToggleView.Update(input);
            if (btnToggleView.IsClicked()) currentView = ViewState::Combat;

            mapRenderer.Render(screen);
            btnToggleView.Render(screen);
            screen.DrawString(screen.GetCenterX() - 6, 2, "[ MAP VIEW ]", COLOR_YELLOW);
        }

        // [공통 UI]
        hud.Render(screen);

        // 디버그 정보 UI
        std::string dbgMouse = "Mouse (X:" + to_string(mouseX) + ", Y:" + to_string(mouseY) + ")";
        std::string dbgState = "L-Click: ";
        if (input.IsLeftClickDown()) dbgState += "[DOWN]";
        else if (input.IsLeftClickUp()) dbgState += "[ UP ]";
        else if (input.IsLeftClick()) dbgState += "[HOLD]";
        else dbgState += "[IDLE]";
        // [복구] 디버그 정보 출력
        screen.DrawString(2, screen.GetHeight() - 54, "Distance: " + to_string((int)dbgDistance), COLOR_WHITE);
        screen.DrawString(2, screen.GetHeight() - 53, "Volume: " + to_string(dbgVolume), COLOR_YELLOW);

        screen.DrawString(2, screen.GetHeight() - 52, dbgMouse, COLOR_WHITE);
        screen.DrawString(20, screen.GetHeight() - 52, dbgState, input.IsLeftClick() ? COLOR_RED : COLOR_WHITE);

        std::string dbgWheel = "Wheel: " + to_string(input.GetWheelDelta());
        screen.DrawString(30, screen.GetHeight() - 51, dbgWheel, COLOR_WHITE); // 휠 상태 출력


        screen.DrawChar(mouseX, mouseY, '+', COLOR_GREEN);
        screen.Render();
        Sleep(16);
    }

    // 소리 리소스 정리
    audio.StopEffect(L"fire_sfx");

    return 0;
}