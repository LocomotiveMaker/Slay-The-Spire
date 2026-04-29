// -----------------------------------------------------------------------------
// @file       CombatSystem.cpp
// @brief      실시간 전투 규칙과 카드 효과 처리 구현부
// -----------------------------------------------------------------------------
#include "CombatSystem.h"
#include <algorithm>
#include <cmath>

namespace {

// 적 의도 창에 쓰이는 짧은 라벨 생성기.
std::string BuildIntentLabel(EnemyIntentType type, int value) {
    switch (type) {
    case EnemyIntentType::Attack:
        return u8"공격 " + std::to_string(value);
    case EnemyIntentType::Defend:
        return u8"방어 " + std::to_string(value);
    case EnemyIntentType::Buff:
        return u8"강화 +" + std::to_string(value);
    default:
        return u8"행위";
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
    currentEnemyIntentBaseIntervalSec(6.0f),
    drawIntervalMultiplier(1.0f),
    playerBlockDecayElapsedSec(0.0f),
    noAttackElapsedSec(0.0f),
    energy(0),
    maxEnergyBonus(0),
    handLimitModifier(0),
    comboCount(0),
    manualDrawCharges(0),
    manualDrawChargesOnEnemyAction(0),
    totalBattleDraws(0),
    totalBattleDiscards(0),
    nextAttackBonus(0),
    basicStrikeBonus(0),
    basicDefendBonus(0),
    discardDamageCounter(0),
    poisonOnAttack(0),
    poisonOnBlockGain(0),
    damageOnBlockGain(0),
    poisonDamageBonusOnEnemyAction(0),
    strengthOnEnemyAction(0),
    warmupStrengthAmount(0),
    bonusStrengthGainWhenEnemyAttacks(0),
    drawsPerStrengthGain(0),
    poisonOnBeingHit(0),
    drawGaugePercentOnDiscard(0),
    extraDrawsOnDiscard(0),
    extraEnergyOnDiscard(0),
    speedAddictionStacks(0),
    energyOnEnemyActionEnd(0),
    attackDelayOnHitSec(0.0f),
    battleOver(false),
    playerWon(false),
    enemyTimedBlockActive(false),
    comboEnabled(false),
    blockDecayActive(false),
    overloadActive(false),
    rng(std::random_device{}()) {
}

float CombatSystem::GetScaledDrawIntervalSec() const {
    return (std::max)(config.minDrawIntervalSec, (config.baseDrawIntervalSec * drawIntervalMultiplier) / speedMultiplier);
}

float CombatSystem::GetScaledEnergyIntervalSec() const {
    return (std::max)(config.minEnergyIntervalSec, config.baseEnergyIntervalSec / speedMultiplier);
}

float CombatSystem::GetScaledEnemyIntentIntervalSec() const {
    return (std::max)(config.minEnemyIntentIntervalSec, currentEnemyIntentBaseIntervalSec / speedMultiplier);
}

float CombatSystem::GetPassiveTimeScale(float timeScale) const {
    return (std::max)(0.0f, timeScale) * speedMultiplier;
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

CardLibrary::CardTextContext CombatSystem::BuildCardTextContext() const {
    CardLibrary::CardTextContext context = {};
    context.comboEnabled = comboEnabled;
    context.comboCount = comboCount;
    context.playerBlock = (player != nullptr) ? player->block : 0;
    context.enemyPoison = (enemy != nullptr) ? enemy->poison : 0;
    context.drawsThisBattle = totalBattleDraws;
    context.discardsThisBattle = totalBattleDiscards;
    context.currentEnergy = energy;
    context.maxEnergy = GetMaxEnergy();
    context.playerDexterity = (player != nullptr) ? player->dexterity : 0;
    context.handCount = static_cast<int>(hand.size());
    return context;
}

void CombatSystem::RefreshCardText(CardData& card) {
    CardLibrary::CardTextContext context = BuildCardTextContext();
    CardLibrary::RefreshRuntimeCardText(card, &context);
}

void CombatSystem::RefreshRuntimeCardTexts() {
    CardLibrary::CardTextContext context = BuildCardTextContext();
    for (CardData& card : drawPile) {
        CardLibrary::RefreshRuntimeCardText(card, &context);
    }
    for (CardData& card : discardPile) {
        CardLibrary::RefreshRuntimeCardText(card, &context);
    }
    for (CardData& card : hand) {
        CardLibrary::RefreshRuntimeCardText(card, &context);
    }
}

bool CombatSystem::TryDrawOne(bool countAsBattleDraw) {
    if (static_cast<int>(hand.size()) >= GetCurrentHandLimit()) {
        return false;
    }

    if (drawPile.empty()) {
        ShuffleDiscardIntoDrawPile();
    }

    if (drawPile.empty()) {
        return false;
    }

    CardData card = drawPile.back();
    drawPile.pop_back();
    card.discardProgress = 0;
    if (countAsBattleDraw) {
        ++totalBattleDraws;
    }

    hand.push_back(card);
    RefreshRuntimeCardTexts();
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
    if (energy > GetMaxEnergy()) {
        energy = GetMaxEnergy();
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

int CombatSystem::BuildAttackDamagePerHit(const CardData& card, int baseDamagePerHit) const {
    int damage = baseDamagePerHit + card.runtimePrimaryModifier;
    if (CardLibrary::IsBasicStrike(card)) {
        damage += basicStrikeBonus;
    }
    if (nextAttackBonus > 0) {
        damage += nextAttackBonus;
    }
    return damage;
}

int CombatSystem::ApplyHitToEnemy(const CardData& card, int baseDamagePerHit, CombatActionResult& result) {
    if (player == nullptr || enemy == nullptr) {
        return 0;
    }

    const int rawDamage = ApplyOutgoingModifiers(*player, *enemy, BuildAttackDamagePerHit(card, baseDamagePerHit));
    const int hpDamage = ApplyDamageToTarget(*enemy, rawDamage);
    if (rawDamage > 0) {
        result.enemyHit = true;
        result.damageToEnemy += hpDamage;
        if (comboEnabled) {
            ++comboCount;
        }
    }
    return hpDamage;
}

void CombatSystem::GainPlayerBlock(int amount, CombatActionResult* actionResult, bool applyDexterity) {
    if (player == nullptr) {
        return;
    }

    int finalAmount = amount;
    if (applyDexterity) {
        finalAmount += player->dexterity;
    }
    finalAmount = (std::max)(0, finalAmount);
    if (finalAmount <= 0) {
        return;
    }

    player->block += finalAmount;
    if (actionResult != nullptr) {
        actionResult->blockGained += finalAmount;
    }

    if (enemy != nullptr) {
        if (poisonOnBlockGain > 0) {
            enemy->poison += poisonOnBlockGain;
        }
        if (damageOnBlockGain > 0) {
            const int hpDamage = ApplyDamageToTarget(*enemy, damageOnBlockGain);
            if (actionResult != nullptr && hpDamage > 0) {
                actionResult->enemyHit = true;
                actionResult->damageToEnemy += hpDamage;
            }
        }
    }
}

void CombatSystem::GainPlayerStrength(int amount, CombatActionResult* actionResult) {
    if (player == nullptr || amount <= 0) {
        return;
    }

    int finalAmount = amount;
    if (currentIntent.type == EnemyIntentType::Attack && bonusStrengthGainWhenEnemyAttacks > 0) {
        finalAmount += bonusStrengthGainWhenEnemyAttacks;
    }

    player->strength += finalAmount;
    if (drawsPerStrengthGain > 0) {
        for (int drawIndex = 0; drawIndex < drawsPerStrengthGain; ++drawIndex) {
            if (TryDrawOne(true)) {
                if (actionResult != nullptr) {
                    ++actionResult->cardsDrawn;
                    actionResult->handChanged = true;
                }
            }
        }
    }
}

void CombatSystem::GainPlayerDexterity(int amount) {
    if (player == nullptr || amount <= 0) {
        return;
    }

    player->dexterity += amount;
}

void CombatSystem::AddCurrentEnemyIntentTime(float amountSec) {
    const float currentInterval = GetScaledEnemyIntentIntervalSec();
    const float currentRemaining = GetEnemyIntentRemainingSec();
    const float cappedRemaining = (std::min)(12.0f, (std::min)(currentInterval, currentRemaining + amountSec));
    enemyIntentElapsedSec = currentInterval - cappedRemaining;
    if (enemyIntentElapsedSec < 0.0f) {
        enemyIntentElapsedSec = 0.0f;
    }
}

void CombatSystem::IncreaseEnemyIntentBaseInterval(float amountSec) {
    const float previousInterval = GetScaledEnemyIntentIntervalSec();
    const float previousRemaining = GetEnemyIntentRemainingSec();

    currentEnemyIntentBaseIntervalSec += amountSec;
    const float cappedBase = config.baseEnemyIntentIntervalSec + 3.0f;
    currentEnemyIntentBaseIntervalSec = (std::min)(cappedBase, currentEnemyIntentBaseIntervalSec);

    const float newInterval = GetScaledEnemyIntentIntervalSec();
    const float nextRemaining = (std::min)(12.0f, (std::min)(newInterval, previousRemaining + (newInterval - previousInterval)));
    enemyIntentElapsedSec = newInterval - nextRemaining;
    if (enemyIntentElapsedSec < 0.0f) {
        enemyIntentElapsedSec = 0.0f;
    }
}

void CombatSystem::ResetEnemyIntentTimer() {
    enemyIntentElapsedSec = 0.0f;
}

void CombatSystem::UpdatePlayerBlockDecay(float deltaTimeSec, float timeScale) {
    if (!blockDecayActive || player == nullptr || player->block <= 0) {
        playerBlockDecayElapsedSec = 0.0f;
        return;
    }

    playerBlockDecayElapsedSec += deltaTimeSec * GetPassiveTimeScale(timeScale);
    while (playerBlockDecayElapsedSec >= 1.0f && player->block > 0) {
        playerBlockDecayElapsedSec -= 1.0f;
        --player->block;
    }
}

void CombatSystem::UpdatePassiveTimers(float deltaTimeSec, float timeScale, CombatFrameResult& result) {
    const float passiveDelta = deltaTimeSec * GetPassiveTimeScale(timeScale);

    if (warmupStrengthAmount > 0) {
        noAttackElapsedSec += passiveDelta;
        while (noAttackElapsedSec >= 3.0f) {
            noAttackElapsedSec -= 3.0f;
            GainPlayerStrength(warmupStrengthAmount);
        }
    }

    for (size_t index = 0; index < delayedEnergyGains.size();) {
        delayedEnergyGains[index].remainingSec -= passiveDelta;
        if (delayedEnergyGains[index].remainingSec <= 0.0f) {
            GainEnergy(delayedEnergyGains[index].amount);
            delayedEnergyGains.erase(delayedEnergyGains.begin() + static_cast<std::ptrdiff_t>(index));
        }
        else {
            ++index;
        }
    }

    UpdatePlayerBlockDecay(deltaTimeSec, timeScale);
    result.battleEnded = battleOver;
}

void CombatSystem::ApplyDiscardEffect(const CardData& card, CombatActionResult& result) {
    switch (card.id) {
    case CardLibrary::Id::Refit:
        if (TryDrawOne(true)) {
            ++result.cardsDrawn;
            result.handChanged = true;
        }
        GainEnergy(1);
        ++result.energyGained;
        break;
    default:
        break;
    }
}

void CombatSystem::AdvanceDrawGaugeByPercent(float ratio, CombatActionResult& result) {
    if (ratio <= 0.0f) {
        return;
    }

    drawElapsedSec += GetScaledDrawIntervalSec() * ratio;
    while (drawElapsedSec >= GetScaledDrawIntervalSec()) {
        drawElapsedSec -= GetScaledDrawIntervalSec();
        if (!TryDrawOne(true)) {
            break;
        }

        ++result.cardsDrawn;
        result.handChanged = true;
    }
}

void CombatSystem::OnCardDiscarded(CardData& discardedCard, CombatActionResult& result, bool countDiscard, bool removedFromHand) {
    if (countDiscard) {
        ++totalBattleDiscards;
        ++discardDamageCounter;

        if (speedAddictionStacks > 0 && enemy != nullptr) {
            while (discardDamageCounter >= 5) {
                discardDamageCounter -= 5;
                const int rawDamage = 15 * speedAddictionStacks;
                const int hpDamage = ApplyDamageToTarget(*enemy, rawDamage);
                if (hpDamage > 0) {
                    result.enemyHit = true;
                    result.damageToEnemy += hpDamage;
                }
            }
        }
    }

    if (removedFromHand) {
        for (CardData& card : hand) {
            if (card.discardRequirement > 0) {
                card.discardProgress = (std::min)(card.discardRequirement, card.discardProgress + 1);
            }
        }
    }

    ApplyDiscardEffect(discardedCard, result);

    if (drawGaugePercentOnDiscard > 0) {
        AdvanceDrawGaugeByPercent(static_cast<float>(drawGaugePercentOnDiscard) / 100.0f, result);
    }

    if (extraEnergyOnDiscard > 0) {
        GainEnergy(extraEnergyOnDiscard);
        result.energyGained += extraEnergyOnDiscard;
    }

    if (extraDrawsOnDiscard > 0) {
        for (int drawIndex = 0; drawIndex < extraDrawsOnDiscard; ++drawIndex) {
            if (TryDrawOne(true)) {
                ++result.cardsDrawn;
                result.handChanged = true;
            }
        }
    }
}

void CombatSystem::DiscardCardFromHand(int handIndex, CombatActionResult& result) {
    if (handIndex < 0 || handIndex >= static_cast<int>(hand.size())) {
        return;
    }

    CardData card = hand[static_cast<size_t>(handIndex)];
    hand.erase(hand.begin() + handIndex);
    if (card.baseReuseCount > 0) {
        card.remainingReuseCount = card.baseReuseCount;
        card.runtimePrimaryModifier = 0;
    }
    discardPile.push_back(card);
    OnCardDiscarded(discardPile.back(), result, true, true);
    result.handChanged = true;
}

void CombatSystem::ExhaustCard(const CardData&) {
}

bool CombatSystem::ReturnReusableCardToHand(CardData card, int handIndex, CombatActionResult& result) {
    if (card.remainingReuseCount <= 0) {
        return false;
    }

    hand.erase(hand.begin() + handIndex);
    --card.remainingReuseCount;
    if (card.id == CardLibrary::Id::RepeatingStab) {
        card.runtimePrimaryModifier += card.secondaryValue;
    }
    hand.insert(hand.begin() + (std::min)(handIndex, static_cast<int>(hand.size())), card);
    result.handChanged = true;
    return true;
}

void CombatSystem::CommitUsedCard(CardData card, int handIndex, CombatActionResult& result) {
    if (card.remainingReuseCount > 0 && ReturnReusableCardToHand(card, handIndex, result)) {
        return;
    }

    hand.erase(hand.begin() + handIndex);
    if (card.type == CardType::Power || card.exhausts) {
        ExhaustCard(card);
        result.handChanged = true;
        return;
    }

    if (card.baseReuseCount > 0) {
        card.remainingReuseCount = card.baseReuseCount;
        card.runtimePrimaryModifier = 0;
    }

    discardPile.push_back(card);
    result.handChanged = true;
}

int CombatSystem::ResolveDiscardAllFromHand(CombatActionResult& result) {
    int discardedCount = 0;
    while (!hand.empty()) {
        DiscardCardFromHand(static_cast<int>(hand.size()) - 1, result);
        ++discardedCount;
    }
    return discardedCount;
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
    if (player == nullptr || enemy == nullptr) {
        return;
    }

    if (!blockDecayActive && player->block > 0) {
        player->block = 0;
    }

    comboCount = 0;

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
        const int poisonDamage = enemy->poison + poisonDamageBonusOnEnemyAction;
        const int hpDamage = ApplyDamageToTarget(*enemy, poisonDamage);
        if (hpDamage > 0) {
            result.enemyHit = true;
            result.damageToEnemy += hpDamage;
        }
        enemy->poison = (std::max)(0, enemy->poison - 1);
    }

    if (strengthOnEnemyAction > 0) {
        GainPlayerStrength(strengthOnEnemyAction);
    }
    if (manualDrawChargesOnEnemyAction > 0) {
        manualDrawCharges += manualDrawChargesOnEnemyAction;
    }
    if (energyOnEnemyActionEnd > 0) {
        GainEnergy(energyOnEnemyActionEnd);
    }

    speedMultiplier *= (1.0f + config.speedGainPerEnemyAction);
    CheckBattleEndState();
    RefreshRuntimeCardTexts();
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

    if (enemyTimedBlockActive && enemy != nullptr) {
        enemy->block = 0;
        enemyTimedBlockActive = false;
    }

    switch (currentIntent.type) {
    case EnemyIntentType::Attack: {
        int rawDamage = ApplyOutgoingModifiers(*enemy, *player, currentIntent.value);
        if (overloadActive) {
            rawDamage *= 2;
        }
        const int hpDamage = ApplyDamageToTarget(*player, rawDamage);
        if (rawDamage > 0) {
            result.playerHit = true;
            result.damageToPlayer += hpDamage;
            if (poisonOnBeingHit > 0 && enemy != nullptr) {
                enemy->poison += poisonOnBeingHit;
            }
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

// 전투 시작 시점에 손패/드로우/자원/상태이상 관련 런타임 값을 전부 초기화한다.
void CombatSystem::StartBattle(const std::vector<CardData>& startingDeck) {
    drawPile = startingDeck;
    discardPile.clear();
    hand.clear();

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
    currentEnemyIntentBaseIntervalSec = config.baseEnemyIntentIntervalSec;
    drawIntervalMultiplier = 1.0f;
    playerBlockDecayElapsedSec = 0.0f;
    noAttackElapsedSec = 0.0f;
    energy = config.startingEnergy;
    maxEnergyBonus = 0;
    handLimitModifier = 0;
    comboCount = 0;
    manualDrawCharges = 0;
    manualDrawChargesOnEnemyAction = 0;
    totalBattleDraws = 0;
    totalBattleDiscards = 0;
    nextAttackBonus = 0;
    basicStrikeBonus = 0;
    basicDefendBonus = 0;
    discardDamageCounter = 0;
    poisonOnAttack = 0;
    poisonOnBlockGain = 0;
    damageOnBlockGain = 0;
    poisonDamageBonusOnEnemyAction = 0;
    strengthOnEnemyAction = 0;
    warmupStrengthAmount = 0;
    bonusStrengthGainWhenEnemyAttacks = 0;
    drawsPerStrengthGain = 0;
    poisonOnBeingHit = 0;
    drawGaugePercentOnDiscard = 0;
    extraDrawsOnDiscard = 0;
    extraEnergyOnDiscard = 0;
    speedAddictionStacks = 0;
    energyOnEnemyActionEnd = 0;
    attackDelayOnHitSec = 0.0f;
    battleOver = false;
    playerWon = false;
    enemyTimedBlockActive = false;
    comboEnabled = false;
    blockDecayActive = false;
    overloadActive = false;
    delayedEnergyGains.clear();

    if (player != nullptr) {
        player->block = 0;
        player->strength = 0;
        player->dexterity = 0;
        player->vulnerable = 0;
        player->weak = 0;
        player->poison = 0;
    }

    if (enemy != nullptr) {
        enemy->block = 0;
        enemy->dexterity = 0;
        enemy->vulnerable = 0;
        enemy->weak = 0;
        enemy->poison = 0;
    }

    RollNextIntent();

    for (int drawIndex = 0; drawIndex < config.startingHandSize; ++drawIndex) {
        if (!TryDrawOne(false)) {
            break;
        }
    }

    RefreshRuntimeCardTexts();
    CheckBattleEndState();
}

// 프레임마다 드로우, 에너지 회복, 적 의도 타이머를 한 번에 갱신한다.
CombatFrameResult CombatSystem::Update(float deltaTimeSec, float timeScale) {
    CombatFrameResult result;
    if (battleOver || player == nullptr || enemy == nullptr) {
        result.battleEnded = battleOver;
        return result;
    }

    UpdatePassiveTimers(deltaTimeSec, timeScale, result);

    const float safeTimeScale = (std::max)(0.0f, timeScale);

    drawElapsedSec += deltaTimeSec * safeTimeScale;
    while (drawElapsedSec >= GetScaledDrawIntervalSec()) {
        drawElapsedSec -= GetScaledDrawIntervalSec();
        if (static_cast<int>(hand.size()) >= GetCurrentHandLimit()) {
            result.overdrawRejected = true;
            continue;
        }

        if (TryDrawOne(true)) {
            result.handChanged = true;
        }
    }

    energyElapsedSec += deltaTimeSec * safeTimeScale;
    while (energyElapsedSec >= GetScaledEnergyIntervalSec()) {
        energyElapsedSec -= GetScaledEnergyIntervalSec();
        if (energy < GetMaxEnergy()) {
            ++energy;
        }
    }

    enemyIntentElapsedSec += deltaTimeSec * safeTimeScale;
    while (!battleOver && enemyIntentElapsedSec >= GetScaledEnemyIntentIntervalSec()) {
        enemyIntentElapsedSec -= GetScaledEnemyIntentIntervalSec();
        ExecuteEnemyIntent(result);
    }

    CheckBattleEndState();
    RefreshRuntimeCardTexts();
    result.battleEnded = battleOver;
    return result;
}

// 카드 한 장 사용의 실제 판정 진입점.
// 비용 검사, 타겟 검사, 효과 적용, 재사용/소멸/버리기까지 여기서 정리한다.
CombatActionResult CombatSystem::TryUseCard(int handIndex, CombatDropTarget target) {
    CombatActionResult result;
    if (battleOver || handIndex < 0 || handIndex >= static_cast<int>(hand.size())) {
        return result;
    }

    CardData card = hand[static_cast<size_t>(handIndex)];
    if (card.cost > energy) {
        result.message = "Not enough energy";
        return result;
    }
    if (card.discardRequirement > 0 && card.discardProgress < card.discardRequirement) {
        result.message = "Locked";
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

    const int comboBeforeUse = comboCount;
    const bool isAttackCard = (card.type == CardType::Attack);

    switch (card.id) {
    case CardLibrary::Id::Strike:
        ApplyHitToEnemy(card, card.primaryValue + basicStrikeBonus, result);
        break;
    case CardLibrary::Id::Defend:
        GainPlayerBlock(card.primaryValue + basicDefendBonus, &result);
        break;

    case CardLibrary::Id::ComboCore:
        comboEnabled = true;
        break;
    case CardLibrary::Id::Jab:
        ApplyHitToEnemy(card, card.primaryValue, result);
        break;
    case CardLibrary::Id::RepeatingStab:
        ApplyHitToEnemy(card, card.primaryValue, result);
        break;
    case CardLibrary::Id::ComboRush:
        ApplyHitToEnemy(card, card.primaryValue + comboBeforeUse, result);
        break;
    case CardLibrary::Id::ComboFinisher:
        ApplyHitToEnemy(card, comboBeforeUse * 3, result);
        comboCount = 0;
        break;
    case CardLibrary::Id::TimingSteal:
        attackDelayOnHitSec += 0.5f;
        break;
    case CardLibrary::Id::BouncingBlade:
        for (int hitIndex = 0; hitIndex < 3; ++hitIndex) {
            ApplyHitToEnemy(card, card.primaryValue, result);
        }
        break;
    case CardLibrary::Id::ExploitOpening:
        enemy->vulnerable += 1;
        result.vulnerableApplied += 1;
        if (comboBeforeUse >= 5) {
            card.remainingReuseCount = (std::max)(card.remainingReuseCount, 1);
        }
        break;
    case CardLibrary::Id::PressureBreathing:
        GainPlayerBlock(5, &result);
        if (comboBeforeUse >= 4) {
            enemy->weak += 1;
        }
        break;
    case CardLibrary::Id::ComboStance:
        ApplyHitToEnemy(card, card.primaryValue, result);
        if (comboBeforeUse >= 5) {
            GainPlayerBlock(card.secondaryValue, &result);
        }
        break;
    case CardLibrary::Id::ResetClock:
        ResetEnemyIntentTimer();
        break;
    case CardLibrary::Id::FrenzyCuts:
        for (int hitIndex = 0; hitIndex < comboBeforeUse; ++hitIndex) {
            ApplyHitToEnemy(card, card.primaryValue, result);
        }
        break;
    case CardLibrary::Id::WhirlBeat:
        ApplyHitToEnemy(card, card.primaryValue, result);
        GainEnergy((std::min)(5, comboBeforeUse / 5));
        result.energyGained += (std::min)(5, comboBeforeUse / 5);
        break;

    case CardLibrary::Id::StrengthTraining:
        GainPlayerStrength(2, &result);
        break;
    case CardLibrary::Id::CrimsonBreath:
        strengthOnEnemyAction += 1;
        break;
    case CardLibrary::Id::HeavyStrike:
        ApplyHitToEnemy(card, card.primaryValue + (player->strength * 2), result);
        break;
    case CardLibrary::Id::Endure:
        GainPlayerBlock(card.primaryValue, &result);
        nextAttackBonus += card.secondaryValue;
        break;
    case CardLibrary::Id::LimitBreak:
        GainPlayerStrength(player->strength, &result);
        break;
    case CardLibrary::Id::BloodAccelerate:
        player->currentHp = (std::max)(0, player->currentHp - card.primaryValue);
        GainPlayerStrength(card.secondaryValue, &result);
        player->vulnerable += card.quaternaryValue;
        for (int drawIndex = 0; drawIndex < card.tertiaryValue; ++drawIndex) {
            if (TryDrawOne(true)) {
                ++result.cardsDrawn;
            }
        }
        break;
    case CardLibrary::Id::Stockpile:
        delayedEnergyGains.push_back({ 5.0f, 3 });
        break;
    case CardLibrary::Id::GiantArm:
        ApplyHitToEnemy(card, card.primaryValue, result);
        break;
    case CardLibrary::Id::Warmup:
        warmupStrengthAmount += 1;
        break;
    case CardLibrary::Id::Crush:
        ApplyHitToEnemy(card, card.primaryValue, result);
        enemy->weak += card.secondaryValue;
        break;
    case CardLibrary::Id::BattleInstinct:
        bonusStrengthGainWhenEnemyAttacks += 1;
        break;
    case CardLibrary::Id::BerserkFlow:
        drawsPerStrengthGain += 1;
        break;
    case CardLibrary::Id::LeechBlade: {
        const int dealtHpDamage = ApplyHitToEnemy(card, card.primaryValue, result);
        player->currentHp = (std::min)(player->maxHp, player->currentHp + static_cast<int>(std::ceil(static_cast<float>(dealtHpDamage) * 0.5f)));
        break;
    }

    case CardLibrary::Id::Condense:
        blockDecayActive = true;
        break;
    case CardLibrary::Id::ShieldBash:
        ApplyHitToEnemy(card, static_cast<int>(std::ceil(static_cast<float>(player->block) * 0.5f)), result);
        break;
    case CardLibrary::Id::FirmPosture:
        GainPlayerBlock(card.primaryValue, &result);
        break;
    case CardLibrary::Id::CompressedDefense: {
        const int previousBlock = player->block;
        player->block = static_cast<int>(std::ceil(static_cast<float>(player->block) * 1.5f));
        const int gained = player->block - previousBlock;
        if (gained > 0) {
            result.blockGained += gained;
            if (poisonOnBlockGain > 0) {
                enemy->poison += poisonOnBlockGain;
            }
            if (damageOnBlockGain > 0) {
                const int hpDamage = ApplyDamageToTarget(*enemy, damageOnBlockGain);
                if (hpDamage > 0) {
                    result.enemyHit = true;
                    result.damageToEnemy += hpDamage;
                }
            }
        }
        break;
    }
    case CardLibrary::Id::EvasionMatrix:
        GainPlayerDexterity(2);
        break;
    case CardLibrary::Id::Invincible:
        GainPlayerBlock(card.primaryValue, &result);
        break;
    case CardLibrary::Id::ReactiveGuard:
        GainPlayerBlock((std::max)(0, card.primaryValue + card.runtimePrimaryModifier), &result);
        card.runtimePrimaryModifier -= 1;
        break;
    case CardLibrary::Id::WeightLoad: {
        int blockAmount = card.primaryValue;
        if (static_cast<int>(hand.size()) >= 6) {
            blockAmount *= 2;
        }
        GainPlayerBlock(blockAmount, &result);
        break;
    }
    case CardLibrary::Id::CounterStance:
        damageOnBlockGain += 3;
        break;
    case CardLibrary::Id::ShieldSpin:
        ApplyHitToEnemy(card, static_cast<int>(std::ceil(static_cast<float>(player->block) * 0.5f)), result);
        break;
    case CardLibrary::Id::GuardConversion:
        if (player->block < 10) {
            result.success = false;
            result.message = "Not enough block";
            GainEnergy(result.energySpent);
            result.energySpent = 0;
            return result;
        }
        player->block -= 10;
        GainPlayerDexterity(1);
        break;
    case CardLibrary::Id::WallCollapse: {
        const int currentBlock = player->block;
        player->block = 0;
        ApplyHitToEnemy(card, static_cast<int>(std::ceil(static_cast<float>(currentBlock) * 1.5f)), result);
        break;
    }

    case CardLibrary::Id::PoisonSting:
        ApplyHitToEnemy(card, card.primaryValue, result);
        enemy->poison += card.secondaryValue;
        break;
    case CardLibrary::Id::VenomCoating:
        poisonOnAttack += 2;
        break;
    case CardLibrary::Id::EvasiveStance:
        GainPlayerBlock(card.primaryValue, &result);
        break;
    case CardLibrary::Id::CorrosiveMist:
        enemy->poison += card.primaryValue;
        break;
    case CardLibrary::Id::ToxicShield:
        poisonOnBlockGain += 2;
        break;
    case CardLibrary::Id::Plague:
        enemy->poison *= 2;
        break;
    case CardLibrary::Id::Catalyst:
        enemy->poison = static_cast<int>(std::ceil(static_cast<float>(enemy->poison) * 1.5f));
        break;
    case CardLibrary::Id::ToxicSkin:
        poisonOnBeingHit += 8;
        break;
    case CardLibrary::Id::SlowDeath:
        poisonDamageBonusOnEnemyAction += 2;
        break;
    case CardLibrary::Id::VenomSlash:
        ApplyHitToEnemy(card, card.primaryValue, result);
        if (enemy->poison > 0) {
            ApplyHitToEnemy(card, card.secondaryValue, result);
        }
        break;
    case CardLibrary::Id::ParalysisPoison:
        enemy->poison += card.primaryValue;
        AddCurrentEnemyIntentTime(2.0f);
        break;
    case CardLibrary::Id::AcidDefense:
        if (enemy->poison > 0) {
            GainPlayerBlock(card.secondaryValue, &result);
        }
        enemy->poison += card.primaryValue;
        break;
    case CardLibrary::Id::RefluxPoison:
        enemy->poison = (std::max)(0, enemy->poison - card.primaryValue);
        for (int drawIndex = 0; drawIndex < card.secondaryValue; ++drawIndex) {
            if (TryDrawOne(true)) {
                ++result.cardsDrawn;
            }
        }
        break;

    case CardLibrary::Id::FastHands:
        for (int drawIndex = 0; drawIndex < 2; ++drawIndex) {
            if (TryDrawOne(true)) {
                ++result.cardsDrawn;
            }
        }
        break;
    case CardLibrary::Id::Refit:
        break;
    case CardLibrary::Id::Overcount:
        ApplyHitToEnemy(card, totalBattleDraws + totalBattleDiscards, result);
        break;
    case CardLibrary::Id::TimeThief:
        IncreaseEnemyIntentBaseInterval(2.0f);
        break;
    case CardLibrary::Id::DeckDigging:
        manualDrawChargesOnEnemyAction += 1;
        break;
    case CardLibrary::Id::FullRedraw: {
        const int redrawCount = static_cast<int>(hand.size());
        hand.erase(hand.begin() + handIndex);
        discardPile.push_back(card);
        OnCardDiscarded(discardPile.back(), result, true, true);
        result.handChanged = true;
        ResolveDiscardAllFromHand(result);
        for (int drawIndex = 0; drawIndex < redrawCount; ++drawIndex) {
            if (TryDrawOne(true)) {
                ++result.cardsDrawn;
            }
        }
        RefreshRuntimeCardTexts();
        CheckBattleEndState();
        return result;
    }
    case CardLibrary::Id::SwiftStrike:
        ApplyHitToEnemy(card, 10, result);
        GainEnergy(1);
        ++result.energyGained;
        break;
    case CardLibrary::Id::ExplosiveStrike:
        basicStrikeBonus += 2;
        break;
    case CardLibrary::Id::ExplosiveDefend:
        basicDefendBonus += 1;
        break;
    case CardLibrary::Id::Recharge: {
        const int refillAmount = GetMaxEnergy() - energy;
        GainEnergy(refillAmount);
        result.energyGained += refillAmount;
        break;
    }
    case CardLibrary::Id::AccelCircuit:
        drawGaugePercentOnDiscard += 35;
        break;
    case CardLibrary::Id::Overload:
        overloadActive = true;
        extraEnergyOnDiscard += 1;
        extraDrawsOnDiscard += 1;
        break;
    case CardLibrary::Id::InstantFortify: {
        const int blockAmount = static_cast<int>(hand.size()) * (card.primaryValue + player->dexterity);
        GainPlayerBlock(blockAmount, &result, false);
        break;
    }
    case CardLibrary::Id::ShredBurst: {
        int discardedCount = 0;
        hand.erase(hand.begin() + handIndex);
        discardPile.push_back(card);
        OnCardDiscarded(discardPile.back(), result, true, true);
        ++discardedCount;
        discardedCount += ResolveDiscardAllFromHand(result);
        for (int hitIndex = 0; hitIndex < discardedCount; ++hitIndex) {
            ApplyHitToEnemy(card, 1, result);
        }
        nextAttackBonus = 0;
        noAttackElapsedSec = 0.0f;
        RefreshRuntimeCardTexts();
        CheckBattleEndState();
        return result;
    }
    case CardLibrary::Id::Blossom:
        ++maxEnergyBonus;
        if (energy > GetMaxEnergy()) {
            energy = GetMaxEnergy();
        }
        break;
    case CardLibrary::Id::ChargeShield: {
        const int spentEnergy = energy;
        SpendEnergy(spentEnergy);
        result.energySpent += spentEnergy;
        GainPlayerBlock(spentEnergy * card.primaryValue, &result);
        break;
    }
    case CardLibrary::Id::HyperDraw:
        drawIntervalMultiplier *= 0.75f;
        --handLimitModifier;
        break;
    case CardLibrary::Id::SpeedAddiction:
        ++speedAddictionStacks;
        break;
    case CardLibrary::Id::PatternRead:
        ++energyOnEnemyActionEnd;
        break;
    default:
        break;
    }

    if (isAttackCard) {
        noAttackElapsedSec = 0.0f;
        if (attackDelayOnHitSec > 0.0f) {
            AddCurrentEnemyIntentTime(attackDelayOnHitSec);
        }
        if (poisonOnAttack > 0) {
            enemy->poison += poisonOnAttack;
        }
        nextAttackBonus = 0;
    }

    CommitUsedCard(card, handIndex, result);
    CheckBattleEndState();
    RefreshRuntimeCardTexts();
    return result;
}

CombatActionResult CombatSystem::TryDiscardCard(int handIndex) {
    CombatActionResult result;
    if (battleOver || handIndex < 0 || handIndex >= static_cast<int>(hand.size())) {
        return result;
    }

    DiscardCardFromHand(handIndex, result);
    result.success = true;
    result.cardDiscarded = true;
    CheckBattleEndState();
    RefreshRuntimeCardTexts();
    return result;
}

CombatActionResult CombatSystem::TryManualDrawFromPile() {
    CombatActionResult result;
    if (battleOver || manualDrawCharges <= 0) {
        return result;
    }

    if (!TryDrawOne(true)) {
        result.message = "Cannot draw";
        return result;
    }

    --manualDrawCharges;
    result.success = true;
    result.handChanged = true;
    result.cardsDrawn = 1;
    RefreshRuntimeCardTexts();
    return result;
}

void CombatSystem::AddEnergy(int amount) {
    GainEnergy(amount);
    RefreshRuntimeCardTexts();
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
