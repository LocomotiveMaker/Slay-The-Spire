// -----------------------------------------------------------------------------
// @file       CombatSystem.cpp
// -----------------------------------------------------------------------------
#include "CombatSystem.h"
#include <algorithm>
#include <cmath>

namespace {

std::string BuildIntentLabel(EnemyIntentType type, int value) {
    switch (type) {
    case EnemyIntentType::Attack:
        return "Attack " + std::to_string(value);
    case EnemyIntentType::Defend:
        return "Block " + std::to_string(value);
    case EnemyIntentType::Buff:
        return "Buff +" + std::to_string(value);
    default:
        return "Intent";
    }
}

} // namespace

CombatSystem::CombatSystem(EntityData* playerData, EntityData* enemyData)
    : player(playerData),
    enemy(enemyData),
    speedMultiplier(1.0f),
    drawElapsedSec(0.0f),
    energyElapsedSec(0.0f),
    enemyIntentElapsedSec(0.0f),
    energy(0),
    battleOver(false),
    playerWon(false),
    enemyTimedBlockActive(false),
    rng(std::random_device{}()) {
}

float CombatSystem::GetScaledDrawIntervalSec() const {
    return (std::max)(config.minDrawIntervalSec, config.baseDrawIntervalSec / speedMultiplier);
}

float CombatSystem::GetScaledEnergyIntervalSec() const {
    return (std::max)(config.minEnergyIntervalSec, config.baseEnergyIntervalSec / speedMultiplier);
}

float CombatSystem::GetScaledEnemyIntentIntervalSec() const {
    return (std::max)(config.minEnemyIntentIntervalSec, config.baseEnemyIntentIntervalSec / speedMultiplier);
}

void CombatSystem::ShuffleDrawPile() {
    std::shuffle(drawPile.begin(), drawPile.end(), rng);
}

void CombatSystem::ShuffleDiscardIntoDrawPile() {
    if (discardPile.empty()) {
        return;
    }

    drawPile.insert(drawPile.end(), discardPile.begin(), discardPile.end());
    discardPile.clear();
    ShuffleDrawPile();
}

bool CombatSystem::TryDrawOne() {
    if (static_cast<int>(hand.size()) >= config.handLimit) {
        return false;
    }

    if (drawPile.empty()) {
        ShuffleDiscardIntoDrawPile();
    }

    if (drawPile.empty()) {
        return false;
    }

    hand.push_back(drawPile.back());
    drawPile.pop_back();
    return true;
}

void CombatSystem::SpendEnergy(int amount) {
    energy -= amount;
    if (energy < 0) {
        energy = 0;
    }
}

void CombatSystem::GainEnergy(int amount) {
    energy += amount;
    if (energy > config.maxEnergy) {
        energy = config.maxEnergy;
    }
}

int CombatSystem::ApplyOutgoingModifiers(const EntityData& attacker, const EntityData& target, int rawDamage) const {
    int modifiedDamage = rawDamage + attacker.strength;
    if (modifiedDamage < 0) {
        modifiedDamage = 0;
    }

    if (attacker.weak > 0) {
        modifiedDamage = static_cast<int>(std::floor(static_cast<float>(modifiedDamage) * 0.75f));
    }

    if (target.vulnerable > 0) {
        modifiedDamage = static_cast<int>(std::round(static_cast<float>(modifiedDamage) * 1.5f));
    }

    return modifiedDamage;
}

int CombatSystem::ApplyDamageToTarget(EntityData& target, int rawDamage) {
    int remainingDamage = rawDamage;
    if (target.block > 0) {
        const int blockedAmount = (std::min)(target.block, remainingDamage);
        target.block -= blockedAmount;
        remainingDamage -= blockedAmount;
    }

    if (remainingDamage > 0) {
        target.currentHp -= remainingDamage;
        if (target.currentHp < 0) {
            target.currentHp = 0;
        }
    }

    return remainingDamage;
}

void CombatSystem::CheckBattleEndState() {
    if (player == nullptr || enemy == nullptr) {
        battleOver = true;
        playerWon = false;
        return;
    }

    if (enemy->currentHp <= 0) {
        enemy->currentHp = 0;
        battleOver = true;
        playerWon = true;
    }
    else if (player->currentHp <= 0) {
        player->currentHp = 0;
        battleOver = true;
        playerWon = false;
    }
}

void CombatSystem::HandleEndOfEnemyAction(CombatFrameResult& result) {
    if (player->block > 0) {
        player->block = 0;
    }

    if (player->vulnerable > 0) {
        --player->vulnerable;
    }
    if (player->weak > 0) {
        --player->weak;
    }
    if (enemy->vulnerable > 0) {
        --enemy->vulnerable;
    }
    if (enemy->weak > 0) {
        --enemy->weak;
    }

    if (enemy->poison > 0) {
        const int poisonDamage = enemy->poison;
        const int hpDamage = ApplyDamageToTarget(*enemy, poisonDamage);
        if (hpDamage > 0) {
            result.enemyHit = true;
            result.damageToEnemy += hpDamage;
        }
        --enemy->poison;
    }

    speedMultiplier *= (1.0f + config.speedGainPerEnemyAction);
    CheckBattleEndState();
}

void CombatSystem::RollNextIntent() {
    std::uniform_int_distribution<int> intentDist(0, 2);
    const int rolledIntent = intentDist(rng);

    switch (rolledIntent) {
    case 0:
        currentIntent.type = EnemyIntentType::Attack;
        currentIntent.value = 7;
        break;
    case 1:
        currentIntent.type = EnemyIntentType::Defend;
        currentIntent.value = 6;
        break;
    case 2:
    default:
        currentIntent.type = EnemyIntentType::Buff;
        currentIntent.value = 1;
        break;
    }

    currentIntent.label = BuildIntentLabel(currentIntent.type, currentIntent.value);
}

void CombatSystem::ExecuteEnemyIntent(CombatFrameResult& result) {
    result.enemyActed = true;

    if (enemyTimedBlockActive) {
        enemy->block = 0;
        enemyTimedBlockActive = false;
    }

    switch (currentIntent.type) {
    case EnemyIntentType::Attack: {
        const int rawDamage = ApplyOutgoingModifiers(*enemy, *player, currentIntent.value);
        const int hpDamage = ApplyDamageToTarget(*player, rawDamage);
        if (rawDamage > 0) {
            result.playerHit = true;
            result.damageToPlayer += hpDamage;
        }
        break;
    }
    case EnemyIntentType::Defend:
        enemy->block += currentIntent.value;
        enemyTimedBlockActive = true;
        result.enemyBlockGained += currentIntent.value;
        break;
    case EnemyIntentType::Buff:
        enemy->strength += currentIntent.value;
        result.enemyStrengthGained += currentIntent.value;
        break;
    default:
        break;
    }

    HandleEndOfEnemyAction(result);

    if (!battleOver) {
        RollNextIntent();
    }
}

void CombatSystem::ApplyDiscardEffect(const CardData& card, CombatActionResult& result) {
    if (card.discardEffectType == CardDiscardEffectType::DrawCardsGainEnergy) {
        for (int drawIndex = 0; drawIndex < card.primaryValue; ++drawIndex) {
            if (TryDrawOne()) {
                ++result.cardsDrawn;
                result.handChanged = true;
            }
        }

        GainEnergy(card.secondaryValue);
        result.energyGained += card.secondaryValue;
    }
}

void CombatSystem::StartBattle(const std::vector<CardData>& startingDeck) {
    drawPile = startingDeck;
    discardPile.clear();
    hand.clear();

    // Early prototype packs can be smaller than the real combat hand loop.
    // Duplicate the battle source locally so realtime draw pacing still works.
    if (!startingDeck.empty()) {
        const size_t minimumDrawSourceSize = static_cast<size_t>(config.startingHandSize + config.handLimit);
        while (drawPile.size() < minimumDrawSourceSize) {
            drawPile.insert(drawPile.end(), startingDeck.begin(), startingDeck.end());
        }
    }

    ShuffleDrawPile();

    speedMultiplier = 1.0f;
    drawElapsedSec = 0.0f;
    energyElapsedSec = 0.0f;
    enemyIntentElapsedSec = 0.0f;
    energy = config.startingEnergy;
    battleOver = false;
    playerWon = false;
    enemyTimedBlockActive = false;

    if (player != nullptr) {
        player->block = 0;
        player->vulnerable = 0;
        player->weak = 0;
        player->poison = 0;
    }

    if (enemy != nullptr) {
        enemy->block = 0;
        enemy->vulnerable = 0;
        enemy->weak = 0;
        enemy->poison = 0;
    }

    RollNextIntent();

    for (int drawIndex = 0; drawIndex < config.startingHandSize; ++drawIndex) {
        if (!TryDrawOne()) {
            break;
        }
    }

    CheckBattleEndState();
}

CombatFrameResult CombatSystem::Update(float deltaTimeSec, float timeScale) {
    CombatFrameResult result;
    if (battleOver || player == nullptr || enemy == nullptr) {
        result.battleEnded = battleOver;
        return result;
    }

    const float safeTimeScale = (std::max)(0.0f, timeScale);

    drawElapsedSec += deltaTimeSec * safeTimeScale;
    while (drawElapsedSec >= GetScaledDrawIntervalSec()) {
        drawElapsedSec -= GetScaledDrawIntervalSec();
        if (static_cast<int>(hand.size()) >= config.handLimit) {
            result.overdrawRejected = true;
            continue;
        }

        if (TryDrawOne()) {
            result.handChanged = true;
        }
    }

    energyElapsedSec += deltaTimeSec * safeTimeScale;
    while (energyElapsedSec >= GetScaledEnergyIntervalSec()) {
        energyElapsedSec -= GetScaledEnergyIntervalSec();
        if (energy < config.maxEnergy) {
            ++energy;
        }
    }

    enemyIntentElapsedSec += deltaTimeSec * safeTimeScale;
    while (!battleOver && enemyIntentElapsedSec >= GetScaledEnemyIntentIntervalSec()) {
        enemyIntentElapsedSec -= GetScaledEnemyIntentIntervalSec();
        ExecuteEnemyIntent(result);
    }

    CheckBattleEndState();
    result.battleEnded = battleOver;
    return result;
}

CombatActionResult CombatSystem::TryUseCard(int handIndex, CombatDropTarget target) {
    CombatActionResult result;
    if (battleOver || handIndex < 0 || handIndex >= static_cast<int>(hand.size())) {
        return result;
    }

    const CardData card = hand[static_cast<size_t>(handIndex)];
    if (card.cost > energy) {
        result.message = "Not enough energy";
        return result;
    }

    const bool validEnemyTarget = (card.targetType == CardTargetType::Enemy && target == CombatDropTarget::Enemy);
    const bool validSelfTarget = (card.targetType == CardTargetType::Self &&
        (target == CombatDropTarget::Player || (card.type != CardType::Attack && target == CombatDropTarget::None)));
    const bool validNoTarget = (card.targetType == CardTargetType::None &&
        (target == CombatDropTarget::None || (card.type != CardType::Attack && target != CombatDropTarget::DiscardPile)));
    if (!validEnemyTarget && !validSelfTarget && !validNoTarget) {
        result.message = "Invalid target";
        return result;
    }

    SpendEnergy(card.cost);
    result.energySpent = card.cost;
    result.success = true;
    result.handChanged = true;

    switch (card.effectType) {
    case CardEffectType::AttackDamage: {
        const int rawDamage = ApplyOutgoingModifiers(*player, *enemy, card.primaryValue);
        const int hpDamage = ApplyDamageToTarget(*enemy, rawDamage);
        result.enemyHit = (rawDamage > 0);
        result.damageToEnemy = hpDamage;
        break;
    }
    case CardEffectType::DefendBlock:
        player->block += card.primaryValue;
        result.blockGained = card.primaryValue;
        break;
    case CardEffectType::ApplyVulnerable:
        enemy->vulnerable += card.primaryValue;
        result.vulnerableApplied = card.primaryValue;
        break;
    case CardEffectType::None:
    default:
        break;
    }

    discardPile.push_back(card);
    hand.erase(hand.begin() + handIndex);

    CheckBattleEndState();
    return result;
}

CombatActionResult CombatSystem::TryDiscardCard(int handIndex) {
    CombatActionResult result;
    if (battleOver || handIndex < 0 || handIndex >= static_cast<int>(hand.size())) {
        return result;
    }

    const CardData card = hand[static_cast<size_t>(handIndex)];
    discardPile.push_back(card);
    hand.erase(hand.begin() + handIndex);

    result.success = true;
    result.handChanged = true;
    result.cardDiscarded = true;

    ApplyDiscardEffect(card, result);
    return result;
}

void CombatSystem::AddEnergy(int amount) {
    GainEnergy(amount);
}

float CombatSystem::GetEnemyIntentRemainingSec() const {
    const float remaining = GetScaledEnemyIntentIntervalSec() - enemyIntentElapsedSec;
    return (std::max)(0.0f, remaining);
}

float CombatSystem::GetEnemyIntentProgress01() const {
    const float interval = GetScaledEnemyIntentIntervalSec();
    if (interval <= 0.0f) {
        return 1.0f;
    }

    return (std::min)(1.0f, enemyIntentElapsedSec / interval);
}

float CombatSystem::GetDragTimeScale() const {
    const float clampedSlowStrength = (std::clamp)(config.dragSlowStrength, 0.0f, 1.0f);
    return 1.0f - clampedSlowStrength;
}
