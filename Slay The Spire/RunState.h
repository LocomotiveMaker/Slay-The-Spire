// -----------------------------------------------------------------------------
// @file       RunState.h
// @brief      런 데이터, 맵 생성, 기록 생성용 인터페이스
// -----------------------------------------------------------------------------
#pragma once
#include "GameData.h"
#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

enum class AppState {
    Title,
    Run,
    Ending
};

enum class TitleOverlayType {
    None,
    Settings,
    Records,
    ConfirmAbandon,
    CharacterSelect,
    AscensionSelect
};

enum class RunSceneType {
    CardPackSelect,
    Room
};

enum class RunOverlayType {
    None,
    Map,
    Deck,
    Settings,
    Confirm,
    Ending
};

enum class RunNodeType {
    Battle,
    Elite,
    Boss,
    Shop,
    Rest,
    Treasure,
    Event
};

enum class RunNodeResultType {
    None,
    Victory,
    Defeat,
    Escape,
    Abandoned,
    Resolved
};

enum class ConfirmActionType {
    None,
    AbandonFromTitle,
    AbandonFromRun,
    SaveAndExit,
    StartOverwrite
};

struct SettingsData {
    int masterVolume = 100;
    int bgmVolume = 100;
    int sfxVolume = 100;
    int gameSpeedPercent = 100;
    bool effectsEnabled = true;
    bool debugMode = false;
    bool enableCharacterSelect = false;
    bool enableAscensionSelect = false;
    std::string customSeedText;
};

struct GlobalStatsData {
    int totalWins = 0;
    int totalLosses = 0;
    int totalEnemiesDefeated = 0;
    int totalCardsUsed = 0;
    int totalCardsDiscarded = 0;
    int totalFloorsClimbed = 0;
    int totalPlayTimeSec = 0;
};

struct RunRecordData {
    bool won = false;
    std::uint32_t seed = 0;
    int reachedFloor = 0;
    int playTimeSec = 0;
    std::string timestampText;
    std::string failureReasonText;
    std::vector<CardData> deckSnapshot;
    std::vector<RelicData> relics;
    std::vector<RunNodeType> visitedNodes;
};

struct RunNodeState {
    int id = -1;
    int floor = 0;
    int x = 0;
    int y = 0;
    RunNodeType type = RunNodeType::Battle;
    std::vector<int> nextNodeIds;
    bool unlocked = false;
    bool visited = false;
    bool completed = false;
    bool isCurrent = false;
    bool reachable = false;
    RunNodeResultType result = RunNodeResultType::None;
};

struct CardPackOption {
    int id = 0;
    CardArchetype archetype = CardArchetype::None;
    std::string title;
    std::string description;
    WORD accentColor = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    std::vector<CardData> cards;
};

enum class ShopOfferType {
    Card,
    Relic,
    Potion,
    Service
};

struct ShopOfferState {
    int id = 0;
    ShopOfferType type = ShopOfferType::Card;
    std::string title;
    std::string description;
    int price = 0;
    bool sold = false;
    CardData card = {};
    RelicData relic = {};
    PotionData potion = {};
};

struct ShopRoomState {
    bool initialized = false;
    bool removeMode = false;
    bool removalUsed = false;
    int removalPrice = 75;
    std::string noticeText;
    std::vector<ShopOfferState> offers;
};

struct RestRoomState {
    bool initialized = false;
    bool resultReady = false;
    std::string noticeText;
    std::string resultText;
};

struct TreasureChoiceState {
    int id = 0;
    std::string title;
    std::string description;
    int goldReward = 0;
    bool grantRelic = false;
    bool grantPotion = false;
    RelicData relic = {};
    PotionData potion = {};
};

struct TreasureRoomState {
    bool initialized = false;
    bool choiceCommitted = false;
    int selectedChoiceId = -1;
    std::string introText;
    std::string resultText;
    std::string noticeText;
    std::vector<TreasureChoiceState> choices;
};

struct EventChoiceState {
    int id = 0;
    std::string label;
    std::string previewText;
    std::string resultTitle;
    std::string resultText;
    int hpDelta = 0;
    int goldDelta = 0;
    bool grantRelic = false;
    bool grantPotion = false;
    bool grantCard = false;
    RelicData relic = {};
    PotionData potion = {};
    CardData card = {};
};

struct EventRoomState {
    bool initialized = false;
    bool choiceCommitted = false;
    int selectedChoiceId = -1;
    std::string title;
    std::string description;
    std::string noticeText;
    std::vector<std::string> artLines;
    std::vector<EventChoiceState> choices;
    std::string resultTitle;
    std::string resultText;
};

struct BattleRewardState {
    bool active = false;
    bool goldAvailable = false;
    int goldAmount = 0;
    bool goldClaimed = false;
    bool potionAvailable = false;
    bool potionClaimed = false;
    bool cardRewardAvailable = false;
    bool cardRewardClaimed = false;
    bool cardSelectionOpen = false;
    std::string title;
    std::string message;
    PotionData potion = {};
    std::vector<CardData> cardChoices;
};

struct BattleRoomState {
    bool initialized = false;
    EntityData enemy = {};
    std::string introText;
    BattleRewardState rewards = {};
};

struct NodeEntrySnapshot {
    bool valid = false;
    int nodeId = -1;
    int floor = 0;
    EntityData player = {};
    int gold = 0;
    std::vector<CardData> deck;
    std::vector<RelicData> relics;
    std::vector<PotionData> potions;
    std::vector<RunNodeType> visitedNodeTypes;
};

struct RunStateData {
    std::uint32_t seed = 0;
    int totalFloors = 0;
    int currentFloor = 0;
    int currentNodeId = -1;
    int gold = 99;
    int playTimeSec = 0;
    int scrollOffset = 0;
    bool finished = false;
    bool won = false;
    bool loseRecordCommitted = false;
    std::string playerName = u8"아이언클래드";
    std::string selectedCardPackTitle;
    CardArchetype selectedCardPackArchetype = CardArchetype::None;
    std::string failureReasonText;
    EntityData player = {};
    std::vector<CardData> deck;
    std::vector<RelicData> relics;
    std::vector<PotionData> potions;
    std::vector<RunNodeState> nodes;
    std::vector<RunNodeType> visitedNodeTypes;
    std::string currentRoomSummaryTitle;
    std::string currentRoomSummaryText;
    RunSceneType scene = RunSceneType::CardPackSelect;
    RunOverlayType overlay = RunOverlayType::None;
    ConfirmActionType pendingConfirm = ConfirmActionType::None;
    RunNodeType currentRoomType = RunNodeType::Battle;
    RunNodeResultType currentRoomResult = RunNodeResultType::None;
    bool roomResolved = false;
    bool pendingExitToTitle = false;
    int selectedStarterPackIndex = -1;
    NodeEntrySnapshot nodeEntrySnapshot = {};
    BattleRoomState battleRoom = {};
    ShopRoomState shopRoom = {};
    RestRoomState restRoom = {};
    TreasureRoomState treasureRoom = {};
    EventRoomState eventRoom = {};
};

std::string RunNodeTypeToString(RunNodeType type);
std::string RunNodeTypeToDisplayName(RunNodeType type);
std::string RunNodeTypeToDescription(RunNodeType type);
std::string RunNodeResultToString(RunNodeResultType result);
std::string BuildTimestampText();

std::vector<CardPackOption> BuildStarterCardPacks();

void CreateNewRun(RunStateData& run, std::uint32_t seed, int screenWidth, int screenHeight);
void ApplyStarterPack(RunStateData& run, const CardPackOption& pack);
void ResetRoomRuntimeState(RunStateData& run);
void PrepareCurrentRoomState(RunStateData& run);
void CaptureNodeEntrySnapshot(RunStateData& run);
void RestoreNodeEntrySnapshot(RunStateData& run);

RunNodeState* FindNodeById(RunStateData& run, int nodeId);
const RunNodeState* FindNodeById(const RunStateData& run, int nodeId);
bool CanEnterNode(const RunStateData& run, int nodeId);
bool EnterNode(RunStateData& run, int nodeId);
void UnlockNextNodes(RunStateData& run, int nodeId);
void ResolveCurrentNode(RunStateData& run, RunNodeResultType result);
void ReopenCurrentNodeIntro(RunStateData& run);
void RefreshReachableNodes(RunStateData& run);

RunRecordData BuildRunRecord(const RunStateData& run, bool won, const std::string& failureReasonText);
