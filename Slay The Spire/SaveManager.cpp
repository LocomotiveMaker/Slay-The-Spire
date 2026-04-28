// -----------------------------------------------------------------------------
// @file       SaveManager.cpp
// -----------------------------------------------------------------------------
#include "SaveManager.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <windows.h>

namespace {

constexpr int kSaveFormatVersion = 2;

std::filesystem::path GetModuleDirectory() {
    wchar_t modulePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    return std::filesystem::path(modulePath).parent_path();
}

std::filesystem::path FindProjectRoot() {
    std::filesystem::path current = GetModuleDirectory();

    for (int depth = 0; depth < 6; ++depth) {
        if (std::filesystem::exists(current / "Slay The Spire.sln") || std::filesystem::exists(current / "AGENT.md")) {
            return current;
        }

        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }

    return GetModuleDirectory();
}

std::filesystem::path GetSettingsPath() {
    return SaveManager::GetSaveDirectory() / "settings.txt";
}

std::filesystem::path GetContinuePath() {
    return SaveManager::GetSaveDirectory() / "continue_run.txt";
}

std::filesystem::path GetRecordsPath() {
    return SaveManager::GetSaveDirectory() / "records.txt";
}

std::filesystem::path GetStatsPath() {
    return SaveManager::GetSaveDirectory() / "stats.txt";
}

void WriteCard(std::ostream& out, const CardData& card) {
    out << "CARD "
        << card.id << ' '
        << std::quoted(card.baseName) << ' '
        << std::quoted(card.name) << ' '
        << card.cost << ' '
        << std::quoted(card.baseDescription) << ' '
        << std::quoted(card.description) << ' '
        << static_cast<int>(card.type) << ' '
        << static_cast<int>(card.targetType) << ' '
        << static_cast<int>(card.effectType) << ' '
        << static_cast<int>(card.discardEffectType) << ' '
        << static_cast<int>(card.rarity) << ' '
        << static_cast<int>(card.archetype) << ' '
        << static_cast<int>(card.starterPackOnly) << ' '
        << static_cast<int>(card.exhausts) << ' '
        << static_cast<int>(card.uniquePower) << ' '
        << card.primaryValue << ' '
        << card.secondaryValue << ' '
        << card.tertiaryValue << ' '
        << card.quaternaryValue << ' '
        << card.upgradeLevel << ' '
        << card.baseReuseCount << ' '
        << card.remainingReuseCount << ' '
        << card.discardRequirement << ' '
        << card.discardProgress << ' '
        << card.timesUsedThisBattle << ' '
        << card.runtimePrimaryModifier << ' '
        << card.runtimeSecondaryModifier << '\n';
}

bool ReadCard(std::istream& in, CardData& card) {
    std::string tag;
    if (!(in >> tag) || tag != "CARD") {
        return false;
    }

    int type = 0;
    int targetType = 0;
    int effectType = 0;
    int discardEffectType = 0;
    int rarity = 0;
    int archetype = 0;
    int starterPackOnly = 0;
    int exhausts = 0;
    int uniquePower = 0;

    in >> card.id
        >> std::quoted(card.baseName)
        >> std::quoted(card.name)
        >> card.cost
        >> std::quoted(card.baseDescription)
        >> std::quoted(card.description)
        >> type
        >> targetType
        >> effectType
        >> discardEffectType
        >> rarity
        >> archetype
        >> starterPackOnly
        >> exhausts
        >> uniquePower
        >> card.primaryValue
        >> card.secondaryValue
        >> card.tertiaryValue
        >> card.quaternaryValue
        >> card.upgradeLevel
        >> card.baseReuseCount
        >> card.remainingReuseCount
        >> card.discardRequirement
        >> card.discardProgress
        >> card.timesUsedThisBattle
        >> card.runtimePrimaryModifier
        >> card.runtimeSecondaryModifier;

    card.type = static_cast<CardType>(type);
    card.targetType = static_cast<CardTargetType>(targetType);
    card.effectType = static_cast<CardEffectType>(effectType);
    card.discardEffectType = static_cast<CardDiscardEffectType>(discardEffectType);
    card.rarity = static_cast<CardRarity>(rarity);
    card.archetype = static_cast<CardArchetype>(archetype);
    card.starterPackOnly = (starterPackOnly != 0);
    card.exhausts = (exhausts != 0);
    card.uniquePower = (uniquePower != 0);
    if (card.baseName.empty()) {
        card.baseName = card.name;
    }
    if (card.baseDescription.empty()) {
        card.baseDescription = card.description;
    }
    return !in.fail();
}

void WriteRelic(std::ostream& out, const RelicData& relic) {
    out << "RELIC " << relic.id << ' ' << std::quoted(relic.name) << ' ' << std::quoted(relic.description) << '\n';
}

bool ReadRelic(std::istream& in, RelicData& relic) {
    std::string tag;
    if (!(in >> tag) || tag != "RELIC") {
        return false;
    }

    in >> relic.id >> std::quoted(relic.name) >> std::quoted(relic.description);
    return !in.fail();
}

void WritePotion(std::ostream& out, const PotionData& potion) {
    out << "POTION " << potion.id << ' ' << std::quoted(potion.name) << ' ' << std::quoted(potion.description) << ' ' << static_cast<int>(potion.battleOnly) << '\n';
}

bool ReadPotion(std::istream& in, PotionData& potion) {
    std::string tag;
    int battleOnly = 0;
    if (!(in >> tag) || tag != "POTION") {
        return false;
    }

    in >> potion.id >> std::quoted(potion.name) >> std::quoted(potion.description) >> battleOnly;
    potion.battleOnly = (battleOnly != 0);
    return !in.fail();
}

void WriteNode(std::ostream& out, const RunNodeState& node) {
    out << "NODE "
        << node.id << ' '
        << node.floor << ' '
        << node.x << ' '
        << node.y << ' '
        << static_cast<int>(node.type) << ' '
        << static_cast<int>(node.unlocked) << ' '
        << static_cast<int>(node.visited) << ' '
        << static_cast<int>(node.completed) << ' '
        << static_cast<int>(node.isCurrent) << ' '
        << static_cast<int>(node.result) << ' '
        << node.nextNodeIds.size();

    for (int nextId : node.nextNodeIds) {
        out << ' ' << nextId;
    }

    out << '\n';
}

bool ReadNode(std::istream& in, RunNodeState& node) {
    std::string tag;
    int type = 0;
    int unlocked = 0;
    int visited = 0;
    int completed = 0;
    int isCurrent = 0;
    int result = 0;
    size_t nextCount = 0;

    if (!(in >> tag) || tag != "NODE") {
        return false;
    }

    in >> node.id >> node.floor >> node.x >> node.y >> type >> unlocked >> visited >> completed >> isCurrent >> result >> nextCount;
    node.type = static_cast<RunNodeType>(type);
    node.unlocked = (unlocked != 0);
    node.visited = (visited != 0);
    node.completed = (completed != 0);
    node.isCurrent = (isCurrent != 0);
    node.result = static_cast<RunNodeResultType>(result);
    node.nextNodeIds.clear();

    for (size_t index = 0; index < nextCount; ++index) {
        int nextId = -1;
        in >> nextId;
        node.nextNodeIds.push_back(nextId);
    }

    return !in.fail();
}

void WriteSnapshot(std::ostream& out, const NodeEntrySnapshot& snapshot) {
    out << "ENTRY_VALID " << static_cast<int>(snapshot.valid) << '\n';
    if (!snapshot.valid) {
        return;
    }

    out << "ENTRY_NODE " << snapshot.nodeId << ' ' << snapshot.floor << '\n';
    out << "ENTRY_PLAYER "
        << snapshot.player.id << ' '
        << std::quoted(snapshot.player.name) << ' '
        << snapshot.player.currentHp << ' '
        << snapshot.player.maxHp << ' '
        << snapshot.player.block << ' '
        << snapshot.player.strength << ' '
        << snapshot.player.dexterity << ' '
        << snapshot.player.vulnerable << ' '
        << snapshot.player.weak << ' '
        << snapshot.player.poison << '\n';
    out << "ENTRY_GOLD " << snapshot.gold << '\n';

    out << "ENTRY_DECK_COUNT " << snapshot.deck.size() << '\n';
    for (const CardData& card : snapshot.deck) {
        WriteCard(out, card);
    }

    out << "ENTRY_RELIC_COUNT " << snapshot.relics.size() << '\n';
    for (const RelicData& relic : snapshot.relics) {
        WriteRelic(out, relic);
    }

    out << "ENTRY_POTION_COUNT " << snapshot.potions.size() << '\n';
    for (const PotionData& potion : snapshot.potions) {
        WritePotion(out, potion);
    }

    out << "ENTRY_VISITED_TYPE_COUNT " << snapshot.visitedNodeTypes.size() << '\n';
    for (RunNodeType type : snapshot.visitedNodeTypes) {
        out << static_cast<int>(type) << '\n';
    }
}

void WriteBattleReward(std::ostream& out, const BattleRewardState& reward) {
    out << "BATTLE_REWARD_ACTIVE " << static_cast<int>(reward.active) << '\n';
    out << "BATTLE_REWARD_GOLD " << static_cast<int>(reward.goldAvailable) << ' ' << reward.goldAmount << ' ' << static_cast<int>(reward.goldClaimed) << '\n';
    out << "BATTLE_REWARD_POTION " << static_cast<int>(reward.potionAvailable) << ' ' << static_cast<int>(reward.potionClaimed) << '\n';
    if (reward.potionAvailable) {
        WritePotion(out, reward.potion);
    }
    out << "BATTLE_REWARD_CARD " << static_cast<int>(reward.cardRewardAvailable) << ' ' << static_cast<int>(reward.cardRewardClaimed) << ' ' << static_cast<int>(reward.cardSelectionOpen) << '\n';
    out << "BATTLE_REWARD_TITLE " << std::quoted(reward.title) << '\n';
    out << "BATTLE_REWARD_MESSAGE " << std::quoted(reward.message) << '\n';
    out << "BATTLE_REWARD_CARD_COUNT " << reward.cardChoices.size() << '\n';
    for (const CardData& card : reward.cardChoices) {
        WriteCard(out, card);
    }
}

bool ReadBattleReward(std::istream& in, BattleRewardState& reward) {
    std::string tag;
    int flagA = 0;
    int flagB = 0;
    int flagC = 0;

    if (!(in >> tag >> flagA) || tag != "BATTLE_REWARD_ACTIVE") {
        return false;
    }
    reward.active = (flagA != 0);

    if (!(in >> tag >> flagA >> reward.goldAmount >> flagB) || tag != "BATTLE_REWARD_GOLD") {
        return false;
    }
    reward.goldAvailable = (flagA != 0);
    reward.goldClaimed = (flagB != 0);

    if (!(in >> tag >> flagA >> flagB) || tag != "BATTLE_REWARD_POTION") {
        return false;
    }
    reward.potionAvailable = (flagA != 0);
    reward.potionClaimed = (flagB != 0);
    if (reward.potionAvailable && !ReadPotion(in, reward.potion)) {
        return false;
    }

    if (!(in >> tag >> flagA >> flagB >> flagC) || tag != "BATTLE_REWARD_CARD") {
        return false;
    }
    reward.cardRewardAvailable = (flagA != 0);
    reward.cardRewardClaimed = (flagB != 0);
    reward.cardSelectionOpen = (flagC != 0);

    if (!(in >> tag >> std::quoted(reward.title)) || tag != "BATTLE_REWARD_TITLE") {
        return false;
    }
    if (!(in >> tag >> std::quoted(reward.message)) || tag != "BATTLE_REWARD_MESSAGE") {
        return false;
    }

    size_t count = 0;
    if (!(in >> tag >> count) || tag != "BATTLE_REWARD_CARD_COUNT") {
        return false;
    }
    reward.cardChoices.clear();
    for (size_t index = 0; index < count; ++index) {
        CardData card = {};
        if (!ReadCard(in, card)) {
            return false;
        }
        reward.cardChoices.push_back(card);
    }

    return !in.fail();
}

void WriteBattleRoom(std::ostream& out, const BattleRoomState& battleRoom) {
    out << "BATTLE_ROOM_INITIALIZED " << static_cast<int>(battleRoom.initialized) << '\n';
    if (!battleRoom.initialized) {
        return;
    }

    out << "BATTLE_ENEMY "
        << battleRoom.enemy.id << ' '
        << std::quoted(battleRoom.enemy.name) << ' '
        << battleRoom.enemy.currentHp << ' '
        << battleRoom.enemy.maxHp << ' '
        << battleRoom.enemy.block << ' '
        << battleRoom.enemy.strength << ' '
        << battleRoom.enemy.dexterity << ' '
        << battleRoom.enemy.vulnerable << ' '
        << battleRoom.enemy.weak << ' '
        << battleRoom.enemy.poison << '\n';
    out << "BATTLE_INTRO " << std::quoted(battleRoom.introText) << '\n';
    WriteBattleReward(out, battleRoom.rewards);
}

bool ReadBattleRoom(std::istream& in, BattleRoomState& battleRoom) {
    std::string tag;
    int initialized = 0;
    if (!(in >> tag >> initialized) || tag != "BATTLE_ROOM_INITIALIZED") {
        return false;
    }
    battleRoom.initialized = (initialized != 0);
    if (!battleRoom.initialized) {
        return true;
    }

    if (!(in >> tag) || tag != "BATTLE_ENEMY") {
        return false;
    }

    in >> battleRoom.enemy.id
        >> std::quoted(battleRoom.enemy.name)
        >> battleRoom.enemy.currentHp
        >> battleRoom.enemy.maxHp
        >> battleRoom.enemy.block
        >> battleRoom.enemy.strength
        >> battleRoom.enemy.dexterity
        >> battleRoom.enemy.vulnerable
        >> battleRoom.enemy.weak
        >> battleRoom.enemy.poison;

    if (!(in >> tag >> std::quoted(battleRoom.introText)) || tag != "BATTLE_INTRO") {
        return false;
    }

    return ReadBattleReward(in, battleRoom.rewards);
}

void WriteRecord(std::ostream& out, const RunRecordData& record) {
    out << "RECORD "
        << static_cast<int>(record.won) << ' '
        << record.seed << ' '
        << record.reachedFloor << ' '
        << record.playTimeSec << ' '
        << std::quoted(record.timestampText) << ' '
        << std::quoted(record.failureReasonText) << '\n';

    out << "RECORD_DECK_COUNT " << record.deckSnapshot.size() << '\n';
    for (const CardData& card : record.deckSnapshot) {
        WriteCard(out, card);
    }

    out << "RECORD_RELIC_COUNT " << record.relics.size() << '\n';
    for (const RelicData& relic : record.relics) {
        WriteRelic(out, relic);
    }

    out << "RECORD_VISITED_COUNT " << record.visitedNodes.size() << '\n';
    for (RunNodeType type : record.visitedNodes) {
        out << "VISITED " << static_cast<int>(type) << '\n';
    }
}

bool ReadRecord(std::istream& in, RunRecordData& record) {
    std::string tag;
    int won = 0;
    if (!(in >> tag) || tag != "RECORD") {
        return false;
    }

    in >> won >> record.seed >> record.reachedFloor >> record.playTimeSec >> std::quoted(record.timestampText) >> std::quoted(record.failureReasonText);
    record.won = (won != 0);

    size_t deckCount = 0;
    size_t relicCount = 0;
    size_t visitedCount = 0;

    in >> tag >> deckCount;
    if (tag != "RECORD_DECK_COUNT") {
        return false;
    }
    record.deckSnapshot.clear();
    for (size_t index = 0; index < deckCount; ++index) {
        CardData card = {};
        if (!ReadCard(in, card)) {
            return false;
        }
        record.deckSnapshot.push_back(card);
    }

    in >> tag >> relicCount;
    if (tag != "RECORD_RELIC_COUNT") {
        return false;
    }
    record.relics.clear();
    for (size_t index = 0; index < relicCount; ++index) {
        RelicData relic = {};
        if (!ReadRelic(in, relic)) {
            return false;
        }
        record.relics.push_back(relic);
    }

    in >> tag >> visitedCount;
    if (tag != "RECORD_VISITED_COUNT") {
        return false;
    }
    record.visitedNodes.clear();
    for (size_t index = 0; index < visitedCount; ++index) {
        int type = 0;
        in >> tag >> type;
        if (tag != "VISITED") {
            return false;
        }
        record.visitedNodes.push_back(static_cast<RunNodeType>(type));
    }

    return !in.fail();
}

void EnsureStatsFromRecord(const RunRecordData& record, GlobalStatsData& stats) {
    if (record.won) {
        ++stats.totalWins;
    }
    else {
        ++stats.totalLosses;
    }

    stats.totalPlayTimeSec += record.playTimeSec;
    stats.totalFloorsClimbed += record.reachedFloor;

    for (RunNodeType type : record.visitedNodes) {
        if (type == RunNodeType::Battle || type == RunNodeType::Elite || type == RunNodeType::Boss) {
            ++stats.totalEnemiesDefeated;
        }
    }
}

RunStateData BuildSanitizedContinueRun(const RunStateData& source) {
    RunStateData sanitized = source;
    sanitized.overlay = RunOverlayType::None;
    sanitized.pendingConfirm = ConfirmActionType::None;
    sanitized.pendingExitToTitle = false;
    sanitized.battleRoom.rewards.cardSelectionOpen = false;
    return sanitized;
}

} // namespace

std::filesystem::path SaveManager::GetProjectRoot() {
    return FindProjectRoot();
}

std::filesystem::path SaveManager::GetSaveDirectory() {
    const std::filesystem::path saveDir = GetProjectRoot() / "Save";
    std::filesystem::create_directories(saveDir);
    return saveDir;
}

SettingsData SaveManager::LoadSettings() {
    SettingsData settings;
    std::ifstream in(GetSettingsPath());
    if (!in.is_open()) {
        return settings;
    }

    int formatVersion = 0;
    std::string tag;
    in >> tag >> formatVersion;
    if (tag != "VERSION" || formatVersion != kSaveFormatVersion) {
        return settings;
    }

    while (in >> tag) {
        if (tag == "MASTER") in >> settings.masterVolume;
        else if (tag == "BGM") in >> settings.bgmVolume;
        else if (tag == "SFX") in >> settings.sfxVolume;
        else if (tag == "GAMESPEED") in >> settings.gameSpeedPercent;
        else if (tag == "EFFECTS") {
            int value = 0;
            in >> value;
            settings.effectsEnabled = (value != 0);
        }
        else if (tag == "DEBUG") {
            int value = 0;
            in >> value;
            settings.debugMode = (value != 0);
        }
        else if (tag == "CHARSELECT") {
            int value = 0;
            in >> value;
            settings.enableCharacterSelect = (value != 0);
        }
        else if (tag == "ASCENSION") {
            int value = 0;
            in >> value;
            settings.enableAscensionSelect = (value != 0);
        }
        else if (tag == "CUSTOMSEED") {
            in >> std::quoted(settings.customSeedText);
        }
    }

    return settings;
}

bool SaveManager::SaveSettings(const SettingsData& settings) {
    std::ofstream out(GetSettingsPath(), std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "VERSION " << kSaveFormatVersion << '\n';
    out << "MASTER " << settings.masterVolume << '\n';
    out << "BGM " << settings.bgmVolume << '\n';
    out << "SFX " << settings.sfxVolume << '\n';
    out << "GAMESPEED " << settings.gameSpeedPercent << '\n';
    out << "EFFECTS " << static_cast<int>(settings.effectsEnabled) << '\n';
    out << "DEBUG " << static_cast<int>(settings.debugMode) << '\n';
    out << "CHARSELECT " << static_cast<int>(settings.enableCharacterSelect) << '\n';
    out << "ASCENSION " << static_cast<int>(settings.enableAscensionSelect) << '\n';
    out << "CUSTOMSEED " << std::quoted(settings.customSeedText) << '\n';
    return true;
}

bool SaveManager::HasContinueRun() {
    return std::filesystem::exists(GetContinuePath());
}

bool SaveManager::LoadContinueRun(RunStateData& run) {
    std::ifstream in(GetContinuePath());
    if (!in.is_open()) {
        return false;
    }

    int formatVersion = 0;
    std::string tag;
    in >> tag >> formatVersion;
    if (tag != "VERSION" || formatVersion != kSaveFormatVersion) {
        return false;
    }

    run = {};

    while (in >> tag) {
        if (tag == "SEED") in >> run.seed;
        else if (tag == "TOTAL_FLOORS") in >> run.totalFloors;
        else if (tag == "CURRENT_FLOOR") in >> run.currentFloor;
        else if (tag == "CURRENT_NODE") in >> run.currentNodeId;
        else if (tag == "GOLD") in >> run.gold;
        else if (tag == "PLAY_TIME") in >> run.playTimeSec;
        else if (tag == "FINISHED") {
            int value = 0;
            in >> value;
            run.finished = (value != 0);
        }
        else if (tag == "WON") {
            int value = 0;
            in >> value;
            run.won = (value != 0);
        }
        else if (tag == "LOSE_RECORD_COMMITTED") {
            int value = 0;
            in >> value;
            run.loseRecordCommitted = (value != 0);
        }
        else if (tag == "PLAYER_NAME") in >> std::quoted(run.playerName);
        else if (tag == "SELECTED_PACK") in >> std::quoted(run.selectedCardPackTitle);
        else if (tag == "SELECTED_PACK_ARCHETYPE") {
            int value = 0;
            in >> value;
            run.selectedCardPackArchetype = static_cast<CardArchetype>(value);
        }
        else if (tag == "FAILURE_REASON") in >> std::quoted(run.failureReasonText);
        else if (tag == "ROOM_SUMMARY_TITLE") in >> std::quoted(run.currentRoomSummaryTitle);
        else if (tag == "ROOM_SUMMARY_TEXT") in >> std::quoted(run.currentRoomSummaryText);
        else if (tag == "PLAYER") {
            in >> run.player.id
                >> std::quoted(run.player.name)
                >> run.player.currentHp
                >> run.player.maxHp
                >> run.player.block
                >> run.player.strength
                >> run.player.dexterity
                >> run.player.vulnerable
                >> run.player.weak
                >> run.player.poison;
        }
        else if (tag == "SCENE") {
            int value = 0;
            in >> value;
            run.scene = static_cast<RunSceneType>(value);
        }
        else if (tag == "OVERLAY") {
            int value = 0;
            in >> value;
            run.overlay = static_cast<RunOverlayType>(value);
        }
        else if (tag == "CONFIRM") {
            int value = 0;
            in >> value;
            run.pendingConfirm = static_cast<ConfirmActionType>(value);
        }
        else if (tag == "ROOM_TYPE") {
            int value = 0;
            in >> value;
            run.currentRoomType = static_cast<RunNodeType>(value);
        }
        else if (tag == "ROOM_RESULT") {
            int value = 0;
            in >> value;
            run.currentRoomResult = static_cast<RunNodeResultType>(value);
        }
        else if (tag == "ROOM_RESOLVED") {
            int value = 0;
            in >> value;
            run.roomResolved = (value != 0);
        }
        else if (tag == "PENDING_EXIT") {
            int value = 0;
            in >> value;
            run.pendingExitToTitle = (value != 0);
        }
        else if (tag == "ENTRY_VALID") {
            int value = 0;
            in >> value;
            run.nodeEntrySnapshot.valid = (value != 0);
        }
        else if (tag == "ENTRY_NODE") {
            in >> run.nodeEntrySnapshot.nodeId >> run.nodeEntrySnapshot.floor;
        }
        else if (tag == "ENTRY_PLAYER") {
            in >> run.nodeEntrySnapshot.player.id
                >> std::quoted(run.nodeEntrySnapshot.player.name)
                >> run.nodeEntrySnapshot.player.currentHp
                >> run.nodeEntrySnapshot.player.maxHp
                >> run.nodeEntrySnapshot.player.block
                >> run.nodeEntrySnapshot.player.strength
                >> run.nodeEntrySnapshot.player.dexterity
                >> run.nodeEntrySnapshot.player.vulnerable
                >> run.nodeEntrySnapshot.player.weak
                >> run.nodeEntrySnapshot.player.poison;
        }
        else if (tag == "ENTRY_GOLD") {
            in >> run.nodeEntrySnapshot.gold;
        }
        else if (tag == "ENTRY_DECK_COUNT") {
            size_t count = 0;
            in >> count;
            run.nodeEntrySnapshot.deck.clear();
            for (size_t index = 0; index < count; ++index) {
                CardData card = {};
                if (!ReadCard(in, card)) {
                    return false;
                }
                run.nodeEntrySnapshot.deck.push_back(card);
            }
        }
        else if (tag == "ENTRY_RELIC_COUNT") {
            size_t count = 0;
            in >> count;
            run.nodeEntrySnapshot.relics.clear();
            for (size_t index = 0; index < count; ++index) {
                RelicData relic = {};
                if (!ReadRelic(in, relic)) {
                    return false;
                }
                run.nodeEntrySnapshot.relics.push_back(relic);
            }
        }
        else if (tag == "ENTRY_POTION_COUNT") {
            size_t count = 0;
            in >> count;
            run.nodeEntrySnapshot.potions.clear();
            for (size_t index = 0; index < count; ++index) {
                PotionData potion = {};
                if (!ReadPotion(in, potion)) {
                    return false;
                }
                run.nodeEntrySnapshot.potions.push_back(potion);
            }
        }
        else if (tag == "ENTRY_VISITED_TYPE_COUNT") {
            size_t count = 0;
            in >> count;
            run.nodeEntrySnapshot.visitedNodeTypes.clear();
            for (size_t index = 0; index < count; ++index) {
                int value = 0;
                in >> value;
                run.nodeEntrySnapshot.visitedNodeTypes.push_back(static_cast<RunNodeType>(value));
            }
        }
        else if (tag == "BATTLE_ROOM_INITIALIZED") {
            int initialized = 0;
            in >> initialized;
            run.battleRoom.initialized = (initialized != 0);
            if (run.battleRoom.initialized) {
                in >> tag;
                if (tag != "BATTLE_ENEMY") {
                    return false;
                }

                in >> run.battleRoom.enemy.id
                    >> std::quoted(run.battleRoom.enemy.name)
                    >> run.battleRoom.enemy.currentHp
                    >> run.battleRoom.enemy.maxHp
                    >> run.battleRoom.enemy.block
                    >> run.battleRoom.enemy.strength
                    >> run.battleRoom.enemy.dexterity
                    >> run.battleRoom.enemy.vulnerable
                    >> run.battleRoom.enemy.weak
                    >> run.battleRoom.enemy.poison;

                in >> tag >> std::quoted(run.battleRoom.introText);
                if (tag != "BATTLE_INTRO") {
                    return false;
                }

                if (!ReadBattleReward(in, run.battleRoom.rewards)) {
                    return false;
                }
            }
        }
        else if (tag == "DECK_COUNT") {
            size_t count = 0;
            in >> count;
            run.deck.clear();
            for (size_t index = 0; index < count; ++index) {
                CardData card = {};
                if (!ReadCard(in, card)) {
                    return false;
                }
                run.deck.push_back(card);
            }
        }
        else if (tag == "RELIC_COUNT") {
            size_t count = 0;
            in >> count;
            run.relics.clear();
            for (size_t index = 0; index < count; ++index) {
                RelicData relic = {};
                if (!ReadRelic(in, relic)) {
                    return false;
                }
                run.relics.push_back(relic);
            }
        }
        else if (tag == "POTION_COUNT") {
            size_t count = 0;
            in >> count;
            run.potions.clear();
            for (size_t index = 0; index < count; ++index) {
                PotionData potion = {};
                if (!ReadPotion(in, potion)) {
                    return false;
                }
                run.potions.push_back(potion);
            }
        }
        else if (tag == "NODE_COUNT") {
            size_t count = 0;
            in >> count;
            run.nodes.clear();
            for (size_t index = 0; index < count; ++index) {
                RunNodeState node = {};
                if (!ReadNode(in, node)) {
                    return false;
                }
                run.nodes.push_back(node);
            }
        }
        else if (tag == "VISITED_TYPE_COUNT") {
            size_t count = 0;
            in >> count;
            run.visitedNodeTypes.clear();
            for (size_t index = 0; index < count; ++index) {
                int value = 0;
                in >> value;
                run.visitedNodeTypes.push_back(static_cast<RunNodeType>(value));
            }
        }
    }

    if (run.currentNodeId >= 0 && !run.roomResolved) {
        RestoreNodeEntrySnapshot(run);
        ReopenCurrentNodeIntro(run);
    }

    run.overlay = RunOverlayType::None;
    run.pendingConfirm = ConfirmActionType::None;
    run.pendingExitToTitle = false;
    run.battleRoom.rewards.cardSelectionOpen = false;
    RefreshReachableNodes(run);

    return true;
}

bool SaveManager::SaveContinueRun(const RunStateData& run) {
    std::ofstream out(GetContinuePath(), std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    const RunStateData sanitized = BuildSanitizedContinueRun(run);

    out << "VERSION " << kSaveFormatVersion << '\n';
    out << "SEED " << sanitized.seed << '\n';
    out << "TOTAL_FLOORS " << sanitized.totalFloors << '\n';
    out << "CURRENT_FLOOR " << sanitized.currentFloor << '\n';
    out << "CURRENT_NODE " << sanitized.currentNodeId << '\n';
    out << "GOLD " << sanitized.gold << '\n';
    out << "PLAY_TIME " << sanitized.playTimeSec << '\n';
    out << "FINISHED " << static_cast<int>(sanitized.finished) << '\n';
    out << "WON " << static_cast<int>(sanitized.won) << '\n';
    out << "LOSE_RECORD_COMMITTED " << static_cast<int>(sanitized.loseRecordCommitted) << '\n';
    out << "PLAYER_NAME " << std::quoted(sanitized.playerName) << '\n';
    out << "SELECTED_PACK " << std::quoted(sanitized.selectedCardPackTitle) << '\n';
    out << "SELECTED_PACK_ARCHETYPE " << static_cast<int>(sanitized.selectedCardPackArchetype) << '\n';
    out << "FAILURE_REASON " << std::quoted(sanitized.failureReasonText) << '\n';
    out << "ROOM_SUMMARY_TITLE " << std::quoted(sanitized.currentRoomSummaryTitle) << '\n';
    out << "ROOM_SUMMARY_TEXT " << std::quoted(sanitized.currentRoomSummaryText) << '\n';
    out << "PLAYER " << sanitized.player.id << ' ' << std::quoted(sanitized.player.name) << ' ' << sanitized.player.currentHp << ' ' << sanitized.player.maxHp << ' '
        << sanitized.player.block << ' ' << sanitized.player.strength << ' ' << sanitized.player.dexterity << ' ' << sanitized.player.vulnerable << ' ' << sanitized.player.weak << ' ' << sanitized.player.poison << '\n';
    out << "SCENE " << static_cast<int>(sanitized.scene) << '\n';
    out << "OVERLAY " << static_cast<int>(sanitized.overlay) << '\n';
    out << "CONFIRM " << static_cast<int>(sanitized.pendingConfirm) << '\n';
    out << "ROOM_TYPE " << static_cast<int>(sanitized.currentRoomType) << '\n';
    out << "ROOM_RESULT " << static_cast<int>(sanitized.currentRoomResult) << '\n';
    out << "ROOM_RESOLVED " << static_cast<int>(sanitized.roomResolved) << '\n';
    out << "PENDING_EXIT " << static_cast<int>(sanitized.pendingExitToTitle) << '\n';
    WriteSnapshot(out, sanitized.nodeEntrySnapshot);
    WriteBattleRoom(out, sanitized.battleRoom);

    out << "DECK_COUNT " << sanitized.deck.size() << '\n';
    for (const CardData& card : sanitized.deck) {
        WriteCard(out, card);
    }

    out << "RELIC_COUNT " << sanitized.relics.size() << '\n';
    for (const RelicData& relic : sanitized.relics) {
        WriteRelic(out, relic);
    }

    out << "POTION_COUNT " << sanitized.potions.size() << '\n';
    for (const PotionData& potion : sanitized.potions) {
        WritePotion(out, potion);
    }

    out << "NODE_COUNT " << sanitized.nodes.size() << '\n';
    for (const RunNodeState& node : sanitized.nodes) {
        WriteNode(out, node);
    }

    out << "VISITED_TYPE_COUNT " << sanitized.visitedNodeTypes.size() << '\n';
    for (RunNodeType type : sanitized.visitedNodeTypes) {
        out << static_cast<int>(type) << '\n';
    }

    return true;
}

bool SaveManager::DeleteContinueRun() {
    const std::filesystem::path continuePath = GetContinuePath();
    if (!std::filesystem::exists(continuePath)) {
        return true;
    }

    return std::filesystem::remove(continuePath);
}

GlobalStatsData SaveManager::LoadGlobalStats() {
    GlobalStatsData stats;
    std::ifstream in(GetStatsPath());
    if (!in.is_open()) {
        return stats;
    }

    std::string tag;
    int formatVersion = 0;
    in >> tag >> formatVersion;
    if (tag != "VERSION" || formatVersion != kSaveFormatVersion) {
        return stats;
    }

    while (in >> tag) {
        if (tag == "TOTAL_WINS") in >> stats.totalWins;
        else if (tag == "TOTAL_LOSSES") in >> stats.totalLosses;
        else if (tag == "TOTAL_ENEMIES") in >> stats.totalEnemiesDefeated;
        else if (tag == "TOTAL_CARD_USED") in >> stats.totalCardsUsed;
        else if (tag == "TOTAL_CARD_DISCARDED") in >> stats.totalCardsDiscarded;
        else if (tag == "TOTAL_FLOORS") in >> stats.totalFloorsClimbed;
        else if (tag == "TOTAL_PLAYTIME") in >> stats.totalPlayTimeSec;
    }

    return stats;
}

bool SaveManager::SaveGlobalStats(const GlobalStatsData& stats) {
    std::ofstream out(GetStatsPath(), std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "VERSION " << kSaveFormatVersion << '\n';
    out << "TOTAL_WINS " << stats.totalWins << '\n';
    out << "TOTAL_LOSSES " << stats.totalLosses << '\n';
    out << "TOTAL_ENEMIES " << stats.totalEnemiesDefeated << '\n';
    out << "TOTAL_CARD_USED " << stats.totalCardsUsed << '\n';
    out << "TOTAL_CARD_DISCARDED " << stats.totalCardsDiscarded << '\n';
    out << "TOTAL_FLOORS " << stats.totalFloorsClimbed << '\n';
    out << "TOTAL_PLAYTIME " << stats.totalPlayTimeSec << '\n';
    return true;
}

std::vector<RunRecordData> SaveManager::LoadRunRecords() {
    std::vector<RunRecordData> records;
    std::ifstream in(GetRecordsPath());
    if (!in.is_open()) {
        return records;
    }

    std::string tag;
    int formatVersion = 0;
    in >> tag >> formatVersion;
    if (tag != "VERSION" || formatVersion != kSaveFormatVersion) {
        return records;
    }

    size_t count = 0;
    in >> tag >> count;
    if (tag != "COUNT") {
        return records;
    }

    for (size_t index = 0; index < count; ++index) {
        RunRecordData record = {};
        if (!ReadRecord(in, record)) {
            break;
        }
        records.push_back(record);
    }

    return records;
}

bool SaveManager::SaveRunRecords(const std::vector<RunRecordData>& records) {
    std::ofstream out(GetRecordsPath(), std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "VERSION " << kSaveFormatVersion << '\n';
    out << "COUNT " << records.size() << '\n';
    for (const RunRecordData& record : records) {
        WriteRecord(out, record);
    }
    return true;
}

bool SaveManager::AppendRunRecord(const RunRecordData& record, GlobalStatsData& stats) {
    std::vector<RunRecordData> records = LoadRunRecords();
    records.insert(records.begin(), record);

    if (records.size() > 30) {
        records.resize(30);
    }

    EnsureStatsFromRecord(record, stats);
    return SaveRunRecords(records) && SaveGlobalStats(stats);
}
