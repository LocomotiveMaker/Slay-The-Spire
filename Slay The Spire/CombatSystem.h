// -----------------------------------------------------------------------------
// @file       CombatSystem.h
// @brief      Real-time combat state and rule execution for the prototype.
// -----------------------------------------------------------------------------
#pragma once
#include "GameData.h"
#include <random>
#include <string>
#include <vector>

enum class CombatDropTarget {
    None,
    Enemy,
    Player,
    DiscardPile
};

enum class EnemyIntentType {
    Attack,
    Defend,
    Buff
};

struct CombatConfig {
    float baseDrawIntervalSec = 1.5f;
    float minDrawIntervalSec = 0.7f;
    float baseEnergyIntervalSec = 2.0f;
    float minEnergyIntervalSec = 0.9f;
    float baseEnemyIntentIntervalSec = 6.0f;
    float minEnemyIntentIntervalSec = 2.0f;
    float speedGainPerEnemyAction = 0.05f;
    float dragSlowStrength = 0.04f;
    int startingHandSize = 5;
    int handLimit = 8;
    int startingEnergy = 3;
    int maxEnergy = 5;
};

struct EnemyIntentState {
    EnemyIntentType type = EnemyIntentType::Attack;
    int value = 0;
    std::string label = "Attack";
};

struct CombatFrameResult {
    bool handChanged = false;
    bool overdrawRejected = false;
    bool enemyActed = false;
    bool playerHit = false;
    bool enemyHit = false;
    bool battleEnded = false;
    int damageToPlayer = 0;
    int damageToEnemy = 0;
    int enemyBlockGained = 0;
    int enemyStrengthGained = 0;
};

struct CombatActionResult {
    bool success = false;
    bool handChanged = false;
    bool cardDiscarded = false;
    bool playerHit = false;
    bool enemyHit = false;
    int energySpent = 0;
    int energyGained = 0;
    int cardsDrawn = 0;
    int damageToEnemy = 0;
    int blockGained = 0;
    int vulnerableApplied = 0;
    std::string message;
};

class CombatSystem {
private:
    EntityData* player;
    EntityData* enemy;
    CombatConfig config;

    std::vector<CardData> drawPile;
    std::vector<CardData> discardPile;
    std::vector<CardData> hand;

    EnemyIntentState currentIntent;

    float speedMultiplier;
    float drawElapsedSec;
    float energyElapsedSec;
    float enemyIntentElapsedSec;

    int energy;
    bool battleOver;
    bool playerWon;
    bool enemyTimedBlockActive;

    std::mt19937 rng;

    float GetScaledDrawIntervalSec() const;
    float GetScaledEnergyIntervalSec() const;
    float GetScaledEnemyIntentIntervalSec() const;

    void ShuffleDrawPile();
    void ShuffleDiscardIntoDrawPile();
    bool TryDrawOne();
    void SpendEnergy(int amount);
    void GainEnergy(int amount);
    int ApplyDamageToTarget(EntityData& target, int rawDamage);
    int ApplyOutgoingModifiers(const EntityData& attacker, const EntityData& target, int rawDamage) const;
    void HandleEndOfEnemyAction(CombatFrameResult& result);
    void RollNextIntent();
    void ExecuteEnemyIntent(CombatFrameResult& result);
    void ApplyDiscardEffect(const CardData& card, CombatActionResult& result);
    void CheckBattleEndState();

public:
    CombatSystem(EntityData* playerData, EntityData* enemyData);

    CombatConfig& GetMutableConfig() { return config; }
    const CombatConfig& GetConfig() const { return config; }

    void StartBattle(const std::vector<CardData>& startingDeck);
    CombatFrameResult Update(float deltaTimeSec, float timeScale = 1.0f);

    CombatActionResult TryUseCard(int handIndex, CombatDropTarget target);
    CombatActionResult TryDiscardCard(int handIndex);

    const std::vector<CardData>& GetHand() const { return hand; }
    const EnemyIntentState& GetCurrentIntent() const { return currentIntent; }

    int GetEnergy() const { return energy; }
    int GetMaxEnergy() const { return config.maxEnergy; }
    int GetDrawPileCount() const { return static_cast<int>(drawPile.size()); }
    int GetDiscardPileCount() const { return static_cast<int>(discardPile.size()); }
    int GetHandLimit() const { return config.handLimit; }
    float GetSpeedMultiplier() const { return speedMultiplier; }
    float GetEnemyIntentRemainingSec() const;
    float GetEnemyIntentProgress01() const;
    float GetDragTimeScale() const;

    bool IsBattleOver() const { return battleOver; }
    bool DidPlayerWin() const { return playerWon; }
};
