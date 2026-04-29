// -----------------------------------------------------------------------------
// @file       CombatSystem.h
// @brief      프로토타입 실시간 전투 상태와 규칙 실행 인터페이스
// -----------------------------------------------------------------------------
#pragma once
#include "CardLibrary.h"
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
    float baseDrawIntervalSec = 3.1f;
    float minDrawIntervalSec = 1.3f;
    float baseEnergyIntervalSec = 3.2f;
    float minEnergyIntervalSec = 1.7f;
    float baseEnemyIntentIntervalSec = 14.0f;
    float minEnemyIntentIntervalSec = 8.0f;
    float speedGainPerEnemyAction = 0.035f;
    float dragSlowStrength = 0.08f;
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
    bool usedPowerCard = false;
    bool playerHit = false;
    bool enemyHit = false;
    int energySpent = 0;
    int energyGained = 0;
    int cardsDrawn = 0;
    int damageToEnemy = 0;
    int blockGained = 0;
    int vulnerableApplied = 0;
    std::string message;
    std::string usedCardName;
};

struct DelayedEnergyGain {
    float remainingSec = 0.0f;
    int amount = 0;
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
    float currentEnemyIntentBaseIntervalSec;
    float drawIntervalMultiplier;
    float playerBlockDecayElapsedSec;
    float noAttackElapsedSec;

    int energy;
    int maxEnergyBonus;
    int handLimitModifier;
    int comboCount;
    int manualDrawCharges;
    int manualDrawChargesOnEnemyAction;
    int totalBattleDraws;
    int totalBattleDiscards;
    int nextAttackBonus;
    int basicStrikeBonus;
    int basicDefendBonus;
    int discardDamageCounter;
    int poisonOnAttack;
    int poisonOnBlockGain;
    int damageOnBlockGain;
    int poisonDamageBonusOnEnemyAction;
    int strengthOnEnemyAction;
    int warmupStrengthAmount;
    int bonusStrengthGainWhenEnemyAttacks;
    int drawsPerStrengthGain;
    int poisonOnBeingHit;
    int drawGaugePercentOnDiscard;
    int extraDrawsOnDiscard;
    int extraEnergyOnDiscard;
    int speedAddictionStacks;
    int energyOnEnemyActionEnd;
    float attackDelayOnHitSec;
    bool battleOver;
    bool playerWon;
    bool enemyTimedBlockActive;
    bool comboEnabled;
    bool blockDecayActive;
    bool overloadActive;

    std::mt19937 rng;
    std::vector<DelayedEnergyGain> delayedEnergyGains;

    float GetScaledDrawIntervalSec() const;
    float GetScaledEnergyIntervalSec() const;
    float GetScaledEnemyIntentIntervalSec() const;
    float GetPassiveTimeScale(float timeScale) const;

    void ShuffleDrawPile();
    void ShuffleDiscardIntoDrawPile();
    bool TryDrawOne(bool countAsBattleDraw = true);
    void RefreshRuntimeCardTexts();
    void RefreshCardText(CardData& card);
    void SpendEnergy(int amount);
    void GainEnergy(int amount);
    int ApplyDamageToTarget(EntityData& target, int rawDamage);
    int ApplyOutgoingModifiers(const EntityData& attacker, const EntityData& target, int rawDamage) const;
    int BuildAttackDamagePerHit(const CardData& card, int baseDamagePerHit) const;
    int ApplyHitToEnemy(const CardData& card, int baseDamagePerHit, CombatActionResult& result);
    void GainPlayerBlock(int amount, CombatActionResult* actionResult = nullptr, bool applyDexterity = true);
    void GainPlayerStrength(int amount, CombatActionResult* actionResult = nullptr);
    void GainPlayerDexterity(int amount);
    void AddCurrentEnemyIntentTime(float amountSec);
    void IncreaseEnemyIntentBaseInterval(float amountSec);
    void ResetEnemyIntentTimer();
    void UpdatePlayerBlockDecay(float deltaTimeSec, float timeScale);
    void UpdatePassiveTimers(float deltaTimeSec, float timeScale, CombatFrameResult& result);
    void OnCardDiscarded(CardData& discardedCard, CombatActionResult& result, bool countDiscard, bool removedFromHand);
    void DiscardCardFromHand(int handIndex, CombatActionResult& result);
    void ExhaustCard(const CardData& card);
    bool ReturnReusableCardToHand(CardData card, int handIndex, CombatActionResult& result);
    void CommitUsedCard(CardData card, int handIndex, CombatActionResult& result);
    int ResolveDiscardAllFromHand(CombatActionResult& result);
    void AdvanceDrawGaugeByPercent(float ratio, CombatActionResult& result);
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
    CombatActionResult TryManualDrawFromPile();
    void AddEnergy(int amount);

    const std::vector<CardData>& GetHand() const { return hand; }
    const EnemyIntentState& GetCurrentIntent() const { return currentIntent; }
    CardLibrary::CardTextContext BuildCardTextContext() const;

    int GetEnergy() const { return energy; }
    int GetMaxEnergy() const { return config.maxEnergy + maxEnergyBonus; }
    int GetDrawPileCount() const { return static_cast<int>(drawPile.size()); }
    int GetDiscardPileCount() const { return static_cast<int>(discardPile.size()); }
    int GetHandLimit() const { return (std::max)(1, config.handLimit); }
    int GetCurrentHandLimit() const { return (std::max)(1, config.handLimit + handLimitModifier); }
    float GetSpeedMultiplier() const { return speedMultiplier; }
    float GetEnemyIntentRemainingSec() const;
    float GetEnemyIntentProgress01() const;
    float GetDragTimeScale() const;
    int GetComboCount() const { return comboCount; }
    bool IsComboEnabled() const { return comboEnabled; }
    int GetPlayerDexterity() const { return player != nullptr ? player->dexterity : 0; }
    int GetManualDrawCharges() const { return manualDrawCharges; }
    int GetTotalBattleDraws() const { return totalBattleDraws; }
    int GetTotalBattleDiscards() const { return totalBattleDiscards; }

    bool IsBattleOver() const { return battleOver; }
    bool DidPlayerWin() const { return playerWon; }
};
