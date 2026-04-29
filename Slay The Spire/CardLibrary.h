// -----------------------------------------------------------------------------
// @file       CardLibrary.h
// @brief      카드 정의, 카드 풀, 동적 설명 문자열 생성 도우미
// -----------------------------------------------------------------------------
#pragma once
#include "GameData.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace CardLibrary {

namespace Id {
constexpr int Strike = 100;
constexpr int Defend = 101;

constexpr int ComboCore = 1000;
constexpr int Jab = 1001;
constexpr int RepeatingStab = 1002;
constexpr int ComboRush = 1003;
constexpr int ComboFinisher = 1004;
constexpr int TimingSteal = 1005;
constexpr int BouncingBlade = 1100;
constexpr int ExploitOpening = 1101;
constexpr int PressureBreathing = 1102;
constexpr int ComboStance = 1103;
constexpr int ResetClock = 1104;
constexpr int FrenzyCuts = 1105;
constexpr int WhirlBeat = 1106;

constexpr int StrengthTraining = 2000;
constexpr int CrimsonBreath = 2001;
constexpr int HeavyStrike = 2002;
constexpr int Endure = 2003;
constexpr int LimitBreak = 2004;
constexpr int BloodAccelerate = 2005;
constexpr int Stockpile = 2100;
constexpr int GiantArm = 2101;
constexpr int Warmup = 2102;
constexpr int Crush = 2103;
constexpr int BattleInstinct = 2104;
constexpr int BerserkFlow = 2105;
constexpr int LeechBlade = 2106;

constexpr int Condense = 3000;
constexpr int ShieldBash = 3001;
constexpr int FirmPosture = 3002;
constexpr int CompressedDefense = 3003;
constexpr int EvasionMatrix = 3004;
constexpr int Invincible = 3100;
constexpr int ReactiveGuard = 3101;
constexpr int WeightLoad = 3102;
constexpr int CounterStance = 3103;
constexpr int ShieldSpin = 3104;
constexpr int GuardConversion = 3105;
constexpr int WallCollapse = 3106;

constexpr int PoisonSting = 4000;
constexpr int VenomCoating = 4001;
constexpr int EvasiveStance = 4002;
constexpr int CorrosiveMist = 4003;
constexpr int ToxicShield = 4004;
constexpr int Plague = 4005;
constexpr int Catalyst = 4006;
constexpr int ToxicSkin = 4100;
constexpr int SlowDeath = 4101;
constexpr int VenomSlash = 4102;
constexpr int ParalysisPoison = 4103;
constexpr int AcidDefense = 4104;
constexpr int RefluxPoison = 4105;

constexpr int FastHands = 5000;
constexpr int Refit = 5001;
constexpr int Overcount = 5002;
constexpr int TimeThief = 5003;
constexpr int DeckDigging = 5004;
constexpr int FullRedraw = 5005;
constexpr int SwiftStrike = 5006;
constexpr int ExplosiveStrike = 5100;
constexpr int ExplosiveDefend = 5101;
constexpr int Recharge = 5102;
constexpr int AccelCircuit = 5103;
constexpr int Overload = 5104;
constexpr int InstantFortify = 5105;
constexpr int ShredBurst = 5106;
constexpr int Blossom = 5107;
constexpr int ChargeShield = 5108;
constexpr int HyperDraw = 5109;
constexpr int SpeedAddiction = 5110;
constexpr int PatternRead = 5111;
}

struct CardTextContext {
    bool comboEnabled = false;
    int comboCount = 0;
    int playerBlock = 0;
    int enemyPoison = 0;
    int drawsThisBattle = 0;
    int discardsThisBattle = 0;
    int currentEnergy = 0;
    int maxEnergy = 0;
    int playerDexterity = 0;
    int handCount = 0;
};

inline int CeilMul(int value, float scale) {
    return static_cast<int>(std::ceil(static_cast<float>(value) * scale));
}

inline std::string CardRarityToString(CardRarity rarity) {
    switch (rarity) {
    case CardRarity::Common:   return u8"일반";
    case CardRarity::Uncommon: return u8"고급";
    case CardRarity::Rare:     return u8"희귀";
    default:                   return u8"카드";
    }
}

inline std::string CardArchetypeToString(CardArchetype archetype) {
    switch (archetype) {
    case CardArchetype::Basic:    return u8"기본";
    case CardArchetype::Combo:    return u8"연타";
    case CardArchetype::Strength: return u8"힘";
    case CardArchetype::Block:    return u8"방어";
    case CardArchetype::Poison:   return u8"독";
    case CardArchetype::Cycle:    return u8"순환";
    default:                      return u8"공용";
    }
}

inline bool IsBasicStrike(const CardData& card) {
    return card.id == Id::Strike;
}

inline bool IsBasicDefend(const CardData& card) {
    return card.id == Id::Defend;
}

inline CardData MakeCard(
    int id,
    const std::string& name,
    int cost,
    const std::string& description,
    CardType type,
    CardTargetType targetType,
    CardArchetype archetype,
    CardRarity rarity,
    int primaryValue = 0,
    int secondaryValue = 0,
    int tertiaryValue = 0,
    int quaternaryValue = 0,
    bool starterPackOnly = false,
    bool exhausts = false,
    int baseReuseCount = 0,
    int discardRequirement = 0) {
    CardData card = {};
    card.id = id;
    card.baseName = name;
    card.name = name;
    card.cost = cost;
    card.baseDescription = description;
    card.description = description;
    card.type = type;
    card.targetType = targetType;
    card.effectType = CardEffectType::Custom;
    card.discardEffectType = CardDiscardEffectType::Custom;
    card.rarity = rarity;
    card.archetype = archetype;
    card.starterPackOnly = starterPackOnly;
    card.exhausts = exhausts;
    card.primaryValue = primaryValue;
    card.secondaryValue = secondaryValue;
    card.tertiaryValue = tertiaryValue;
    card.quaternaryValue = quaternaryValue;
    card.baseReuseCount = baseReuseCount;
    card.remainingReuseCount = baseReuseCount;
    card.discardRequirement = discardRequirement;
    card.discardProgress = 0;
    return card;
}

inline std::string BuildRuntimeName(const CardData& card) {
    std::string name = card.baseName.empty() ? card.name : card.baseName;
    if (card.baseReuseCount > 0) {
        if (card.remainingReuseCount >= 2) {
            name += u8" {재사용 2}";
        }
        else if (card.remainingReuseCount == 1) {
            name += u8" {재사용 1}";
        }
        else {
            name += u8" {닳음}";
        }
    }
    return name;
}

inline std::string BuildRuntimeDescription(const CardData& card, const CardTextContext* context = nullptr) {
    const int comboCount = (context != nullptr) ? context->comboCount : 0;
    const int playerBlock = (context != nullptr) ? context->playerBlock : 0;
    const int poisonAmount = (context != nullptr) ? context->enemyPoison : 0;
    const int drawDiscardTotal = (context != nullptr) ? (context->drawsThisBattle + context->discardsThisBattle) : 0;
    const int currentEnergy = (context != nullptr) ? context->currentEnergy : 0;
    const int dexterity = (context != nullptr) ? context->playerDexterity : 0;

    switch (card.id) {
    case Id::Strike:
        return u8"적에게 " + std::to_string(card.primaryValue) + u8" 피해를 줍니다.";
    case Id::Defend:
        return u8"방어도 " + std::to_string(card.primaryValue) + u8"를 얻습니다.";
    case Id::ComboCore:
        return u8"이제부터 콤보를 가질 수 있습니다. 콤보는 실제 타격마다 1 증가하고 적 행위마다 초기화됩니다.";
    case Id::Jab:
        return u8"적에게 " + std::to_string(card.primaryValue) + u8" 피해를 줍니다.";
    case Id::RepeatingStab:
        return u8"적에게 " + std::to_string(card.primaryValue + card.runtimePrimaryModifier) + u8" 피해를 줍니다. 사용 후 손패로 돌아오며 재사용이 줄어듭니다.";
    case Id::ComboRush:
        return u8"적에게 " + std::to_string(card.primaryValue) + u8" 피해를 주고 콤보 수만큼 추가 피해를 줍니다. 현재 추가 피해: " + std::to_string(comboCount);
    case Id::ComboFinisher:
        return u8"콤보를 모두 소모하고 소모한 콤보 * 3 피해를 줍니다. 현재 예상 피해: " + std::to_string(comboCount * 3);
    case Id::TimingSteal:
        return u8"공격할 때마다 적의 현재 행위 시간을 0.5초 늘립니다.";
    case Id::BouncingBlade:
        return u8"무작위 적에게 3번 튕기며 각각 " + std::to_string(card.primaryValue) + u8" 피해를 줍니다.";
    case Id::ExploitOpening:
        return u8"적에게 취약 1을 부여합니다. 콤보가 5 이상이면 자신에게 재사용 1을 부여합니다.";
    case Id::PressureBreathing:
        return u8"방어도 5를 얻습니다. 콤보가 4 이상이면 적에게 약화 1을 부여합니다.";
    case Id::ComboStance:
        return u8"적에게 6 피해를 줍니다. 콤보가 5 이상이면 방어도 6도 얻습니다.";
    case Id::ResetClock:
        return u8"적의 현재 행위 타이머를 현재 최대치로 되돌립니다.";
    case Id::FrenzyCuts:
        return u8"콤보 수만큼 " + std::to_string(card.primaryValue) + u8" 피해를 반복합니다. 각 타격도 콤보를 올립니다.";
    case Id::WhirlBeat:
        return u8"적에게 25 피해를 줍니다. 콤보 5마다 에너지 1을 환급합니다. 현재 환급: " + std::to_string((std::min)(5, comboCount / 5));

    case Id::StrengthTraining:
        return u8"힘 +2.";
    case Id::CrimsonBreath:
        return u8"적 행동마다 힘 +1.";
    case Id::HeavyStrike:
        return u8"적에게 12 피해를 주고 현재 힘 효과를 3배로 적용합니다.";
    case Id::Endure:
        return u8"방어도 8을 얻고 다음 공격 카드의 모든 타격 피해가 +3 됩니다.";
    case Id::LimitBreak:
        return u8"현재 힘을 2배로 만듭니다. 1회용.";
    case Id::BloodAccelerate:
        return u8"체력 8을 잃고 힘 2, 카드 2장 드로우, 취약 2를 얻습니다.";
    case Id::Stockpile:
        return u8"5초 후 에너지 3을 얻습니다.";
    case Id::GiantArm:
        return u8"적에게 20 피해를 줍니다.";
    case Id::Warmup:
        return u8"공격하지 않은 시간 3초마다 힘 +1.";
    case Id::Crush:
        return u8"적에게 10 피해를 주고 약화 2를 부여합니다.";
    case Id::BattleInstinct:
        return u8"적 의도가 공격일 때 힘을 얻는 양이 1 증가합니다.";
    case Id::BerserkFlow:
        return u8"힘을 얻을 때마다 카드 1장을 뽑습니다.";
    case Id::LeechBlade:
        return u8"적에게 14 피해를 주고 실제 체력 피해의 50%를 회복합니다.";

    case Id::Condense:
        return u8"방어도가 즉시 사라지지 않고 1초마다 1씩 줄어듭니다.";
    case Id::ShieldBash:
        return u8"현재 방어도의 50%만큼 피해를 줍니다. 현재 예상 피해: " + std::to_string(CeilMul(playerBlock, 0.5f));
    case Id::FirmPosture:
        return u8"방어도 12를 얻습니다.";
    case Id::CompressedDefense:
        return u8"현재 방어도를 1.5배로 만들고 손패로 한 번 되돌아옵니다. 현재 예상 방어도: " + std::to_string(CeilMul(playerBlock, 1.5f));
    case Id::EvasionMatrix:
        return u8"민첩 +2.";
    case Id::Invincible:
        return u8"방어도 30을 얻습니다. 1회용.";
    case Id::ReactiveGuard:
        return u8"방어도 " + std::to_string((std::max)(0, card.primaryValue + card.runtimePrimaryModifier)) + u8"을 얻습니다. 이번 전투에서 사용할 때마다 값이 1씩 줄어듭니다.";
    case Id::WeightLoad:
        return u8"방어도 4를 얻습니다. 손패가 6장 이상이면 2배를 얻습니다.";
    case Id::CounterStance:
        return u8"방어도를 얻을 때마다 적에게 3 피해를 줍니다.";
    case Id::ShieldSpin:
        return u8"현재 방어도 절반만큼 피해를 줍니다. 현재 예상 피해: " + std::to_string(CeilMul(playerBlock, 0.5f));
    case Id::GuardConversion:
        return u8"방어도가 10 이상일 때만 사용 가능. 방어도 10을 소비하고 민첩 1을 얻습니다.";
    case Id::WallCollapse:
        return u8"방어도를 모두 소모해 그 수치의 1.5배 피해를 줍니다. 현재 예상 피해: " + std::to_string(CeilMul(playerBlock, 1.5f));

    case Id::PoisonSting:
        return u8"적에게 4 피해와 독 2를 줍니다.";
    case Id::VenomCoating:
        return u8"이번 전투 동안 다음 공격마다 독 2를 부여합니다.";
    case Id::EvasiveStance:
        return u8"방어도 9를 얻습니다.";
    case Id::CorrosiveMist:
        return u8"적에게 독 5를 부여합니다.";
    case Id::ToxicShield:
        return u8"방어도를 얻을 때마다 적에게 독 2를 부여합니다.";
    case Id::Plague:
        return u8"적의 독을 2배로 만듭니다. 1회용.";
    case Id::Catalyst:
        return u8"적의 독을 1.5배로 만듭니다.";
    case Id::ToxicSkin:
        return u8"피격당할 때마다 적에게 독 8을 부여합니다.";
    case Id::SlowDeath:
        return u8"적 행동 시 독 피해가 2 추가됩니다.";
    case Id::VenomSlash:
        return u8"적에게 5 피해를 줍니다. 적이 독 상태라면 5 피해를 한 번 더 줍니다. 현재 독: " + std::to_string(poisonAmount);
    case Id::ParalysisPoison:
        return u8"적에게 독 3을 부여하고 현재 행위 시간을 2초 늘립니다.";
    case Id::AcidDefense:
        return u8"적에게 독 6을 부여합니다. 적이 이미 독 상태라면 방어도 8도 얻습니다.";
    case Id::RefluxPoison:
        return u8"적의 독을 2 줄이고 카드 2장을 뽑습니다.";

    case Id::FastHands:
        return u8"카드 2장을 뽑습니다.";
    case Id::Refit:
        return u8"버릴 때 카드 1장을 뽑고 에너지 1을 얻습니다.";
    case Id::Overcount:
        return u8"이번 전투의 드로우 횟수와 버리기 횟수의 합만큼 피해를 줍니다. 현재 예상 피해: " + std::to_string(drawDiscardTotal);
    case Id::TimeThief:
        return u8"적의 행위 주기 최대치를 2초 늘리고 현재 타이머도 함께 늦춥니다.";
    case Id::DeckDigging:
        return u8"적 행위 발동마다 성급 +1. 성급은 뽑을 카드 더미에서 직접 카드를 꺼내는 횟수입니다.";
    case Id::FullRedraw:
        return u8"손패를 모두 버리고 같은 수만큼 다시 뽑습니다.";
    case Id::SwiftStrike:
        return u8"손에 들어온 뒤 카드가 3번 버려져야 사용 가능합니다. 현재 진행: " + std::to_string(card.discardProgress) + u8"/" + std::to_string(card.discardRequirement) + u8". 피해 10, 에너지 +1.";
    case Id::ExplosiveStrike:
        return u8"이번 전투 동안 기본 타격 카드의 피해를 2 올립니다.";
    case Id::ExplosiveDefend:
        return u8"이번 전투 동안 기본 수비 카드의 방어도를 1 올립니다.";
    case Id::Recharge:
        return u8"에너지를 모두 충전합니다. 1회용.";
    case Id::AccelCircuit:
        return u8"카드를 버릴 때마다 드로우 게이지를 35% 채웁니다.";
    case Id::Overload:
        return u8"카드를 버릴 때마다 에너지 1을 얻고 카드 1장을 뽑습니다. 대신 받는 피해가 2배가 됩니다.";
    case Id::InstantFortify:
        return u8"손패 수 * 2 만큼의 방어도를 얻습니다. 민첩은 손패 수에 비례해 추가됩니다. 현재 예상 방어도: " +
            std::to_string((context != nullptr) ? (context->handCount * (card.primaryValue + context->playerDexterity)) : 0);
    case Id::ShredBurst:
        return u8"손패를 모두 버리고 버린 수만큼 각각 피해를 줍니다.";
    case Id::Blossom:
        return u8"이번 전투 동안 최대 에너지 +1.";
    case Id::ChargeShield:
        return u8"남은 에너지를 모두 소비하고 소비한 에너지 * 5의 방어도를 얻습니다. 현재 예상 방어도: " + std::to_string(currentEnergy * 5);
    case Id::HyperDraw:
        return u8"드로우 속도를 높이는 대신 최대 손패가 1 줄어듭니다.";
    case Id::SpeedAddiction:
        return u8"카드를 5장 버릴 때마다 모든 적에게 15 피해를 줍니다.";
    case Id::PatternRead:
        return u8"적의 행위가 끝날 때마다 에너지 1을 얻습니다.";
    default:
        return card.baseDescription.empty() ? card.description : card.baseDescription;
    }
}

inline void RefreshRuntimeCardText(CardData& card, const CardTextContext* context = nullptr) {
    card.name = BuildRuntimeName(card);
    card.description = BuildRuntimeDescription(card, context);
}

inline CardData CreateCardById(int id) {
    switch (id) {
    case Id::Strike:
        return MakeCard(id, u8"타격", 1, u8"적에게 6 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Basic, CardRarity::Common, 6);
    case Id::Defend:
        return MakeCard(id, u8"수비", 1, u8"방어도 5를 얻습니다.", CardType::Skill, CardTargetType::Self, CardArchetype::Basic, CardRarity::Common, 5);

    case Id::ComboCore:
        return MakeCard(id, u8"콤보", 3, u8"이제부터 콤보를 가질 수 있습니다.", CardType::Power, CardTargetType::None, CardArchetype::Combo, CardRarity::Rare, 0, 0, 0, 0, true, true);
    case Id::Jab:
        return MakeCard(id, u8"잽", 0, u8"적에게 6 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Combo, CardRarity::Common, 6, 1, 0, 0, true);
    case Id::RepeatingStab:
        return MakeCard(id, u8"연속 찌르기", 1, u8"적에게 5 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Combo, CardRarity::Uncommon, 5, 3, 0, 0, true, false, 2);
    case Id::ComboRush:
        return MakeCard(id, u8"몰아치기", 1, u8"콤보 수에 비례해 피해가 증가합니다.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Combo, CardRarity::Common, 6, 1, 0, 0, true);
    case Id::ComboFinisher:
        return MakeCard(id, u8"마무리 공격", 3, u8"콤보를 모두 소모하고 큰 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Combo, CardRarity::Rare, 3, 0, 0, 0, true);
    case Id::TimingSteal:
        return MakeCard(id, u8"타이밍 뺏기", 2, u8"공격할 때마다 적의 시간을 빼앗습니다.", CardType::Power, CardTargetType::None, CardArchetype::Combo, CardRarity::Rare, 1, 0, 0, 0, true, true);
    case Id::BouncingBlade:
        return MakeCard(id, u8"튕기는 칼날", 1, u8"무작위 적에게 3번 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Combo, CardRarity::Common, 2, 3);
    case Id::ExploitOpening:
        return MakeCard(id, u8"빈틈 포착", 1, u8"취약 1. 콤보 5 이상이면 재사용을 얻습니다.", CardType::Skill, CardTargetType::Enemy, CardArchetype::Combo, CardRarity::Uncommon, 1);
    case Id::PressureBreathing:
        return MakeCard(id, u8"호흡 압박", 1, u8"방어도 5. 콤보 4 이상이면 약화 1.", CardType::Skill, CardTargetType::Self, CardArchetype::Combo, CardRarity::Common, 5, 1);
    case Id::ComboStance:
        return MakeCard(id, u8"연타 자세", 1, u8"6 피해. 콤보 5 이상이면 방어도 6.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Combo, CardRarity::Common, 6, 6);
    case Id::ResetClock:
        return MakeCard(id, u8"칼끝 유지", 2, u8"적의 현재 행위 타이머를 초기화합니다.", CardType::Skill, CardTargetType::Enemy, CardArchetype::Combo, CardRarity::Rare);
    case Id::FrenzyCuts:
        return MakeCard(id, u8"난도질", 3, u8"콤보 수만큼 반복 공격합니다.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Combo, CardRarity::Rare, 2);
    case Id::WhirlBeat:
        return MakeCard(id, u8"휘몰아치는 박자", 5, u8"강한 일격 후 콤보에 따라 에너지를 환급합니다.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Combo, CardRarity::Rare, 25);

    case Id::StrengthTraining:
        return MakeCard(id, u8"근력 단련", 1, u8"힘 +2.", CardType::Skill, CardTargetType::Self, CardArchetype::Strength, CardRarity::Common, 2, 0, 0, 0, true);
    case Id::CrimsonBreath:
        return MakeCard(id, u8"붉은 호흡", 1, u8"적 행동마다 힘 +1.", CardType::Power, CardTargetType::None, CardArchetype::Strength, CardRarity::Uncommon, 1, 0, 0, 0, true, true);
    case Id::HeavyStrike:
        return MakeCard(id, u8"무거운 일격", 3, u8"힘 효과를 크게 받는 공격입니다.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Strength, CardRarity::Common, 12, 3, 0, 0, true);
    case Id::Endure:
        return MakeCard(id, u8"버티기", 1, u8"방어도 8, 다음 공격 피해 +3.", CardType::Skill, CardTargetType::Self, CardArchetype::Strength, CardRarity::Common, 8, 3, 0, 0, true);
    case Id::LimitBreak:
        return MakeCard(id, u8"한계돌파", 2, u8"힘 2배. 1회용.", CardType::Skill, CardTargetType::Self, CardArchetype::Strength, CardRarity::Rare, 0, 0, 0, 0, true, true);
    case Id::BloodAccelerate:
        return MakeCard(id, u8"피의 가속", 1, u8"체력 소모 후 힘과 드로우를 얻습니다.", CardType::Skill, CardTargetType::Self, CardArchetype::Strength, CardRarity::Uncommon, 8, 2, 2, 2, true);
    case Id::Stockpile:
        return MakeCard(id, u8"축적", 1, u8"5초 후 에너지 3.", CardType::Skill, CardTargetType::Self, CardArchetype::Strength, CardRarity::Uncommon, 5, 3);
    case Id::GiantArm:
        return MakeCard(id, u8"거인의 팔", 3, u8"적에게 20 피해를 줍니다.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Strength, CardRarity::Common, 20);
    case Id::Warmup:
        return MakeCard(id, u8"예열", 1, u8"공격하지 않은 시간마다 힘을 얻습니다.", CardType::Power, CardTargetType::None, CardArchetype::Strength, CardRarity::Uncommon, 3, 1, 0, 0, false, true);
    case Id::Crush:
        return MakeCard(id, u8"짓누르기", 2, u8"피해와 약화를 겁니다.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Strength, CardRarity::Common, 10, 2);
    case Id::BattleInstinct:
        return MakeCard(id, u8"전투 본능", 1, u8"적 공격 의도 중 힘 획득량 +1.", CardType::Power, CardTargetType::None, CardArchetype::Strength, CardRarity::Uncommon, 1, 0, 0, 0, false, true);
    case Id::BerserkFlow:
        return MakeCard(id, u8"폭주", 3, u8"힘을 얻을 때마다 카드 1장 드로우.", CardType::Power, CardTargetType::None, CardArchetype::Strength, CardRarity::Rare, 1, 0, 0, 0, false, true);
    case Id::LeechBlade:
        return MakeCard(id, u8"흡수 베기", 3, u8"피해 후 그 절반만큼 회복합니다.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Strength, CardRarity::Rare, 14);

    case Id::Condense:
        return MakeCard(id, u8"응축", 4, u8"방어도가 서서히 감소합니다.", CardType::Power, CardTargetType::None, CardArchetype::Block, CardRarity::Rare, 1, 0, 0, 0, true, true);
    case Id::ShieldBash:
        return MakeCard(id, u8"방패 밀기", 1, u8"현재 방어도 비례 피해.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Block, CardRarity::Common, 50, 0, 0, 0, true);
    case Id::FirmPosture:
        return MakeCard(id, u8"굳건한 자세", 2, u8"방어도 12.", CardType::Skill, CardTargetType::Self, CardArchetype::Block, CardRarity::Common, 12, 0, 0, 0, true);
    case Id::CompressedDefense:
        return MakeCard(id, u8"압축 방어", 2, u8"현재 방어도 1.5배, 재사용 1.", CardType::Skill, CardTargetType::Self, CardArchetype::Block, CardRarity::Uncommon, 150, 0, 0, 0, true, false, 1);
    case Id::EvasionMatrix:
        return MakeCard(id, u8"회피", 2, u8"민첩 +2.", CardType::Power, CardTargetType::None, CardArchetype::Block, CardRarity::Uncommon, 2, 0, 0, 0, true, true);
    case Id::Invincible:
        return MakeCard(id, u8"무적", 4, u8"방어도 30. 1회용.", CardType::Skill, CardTargetType::Self, CardArchetype::Block, CardRarity::Rare, 30, 0, 0, 0, false, true);
    case Id::ReactiveGuard:
        return MakeCard(id, u8"즉응 방패", 0, u8"방어도 8. 사용할 때마다 값이 줄어듭니다.", CardType::Skill, CardTargetType::Self, CardArchetype::Block, CardRarity::Common, 8);
    case Id::WeightLoad:
        return MakeCard(id, u8"무게 싣기", 0, u8"손패가 많을수록 효율이 오릅니다.", CardType::Skill, CardTargetType::Self, CardArchetype::Block, CardRarity::Common, 4, 6);
    case Id::CounterStance:
        return MakeCard(id, u8"반격 태세", 3, u8"방어도를 얻을 때마다 적에게 3 피해.", CardType::Power, CardTargetType::None, CardArchetype::Block, CardRarity::Rare, 3, 0, 0, 0, false, true);
    case Id::ShieldSpin:
        return MakeCard(id, u8"방패 회전", 2, u8"현재 방어도 절반만큼 전체 피해.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Block, CardRarity::Uncommon, 50);
    case Id::GuardConversion:
        return MakeCard(id, u8"방어 전환", 1, u8"방어도 10을 민첩 1로 바꿉니다.", CardType::Skill, CardTargetType::Self, CardArchetype::Block, CardRarity::Uncommon, 10, 1);
    case Id::WallCollapse:
        return MakeCard(id, u8"성벽 붕괴", 3, u8"방어도를 모두 소모해 1.5배 피해.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Block, CardRarity::Rare, 150);

    case Id::PoisonSting:
        return MakeCard(id, u8"독침", 1, u8"4 피해, 독 2.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Poison, CardRarity::Common, 4, 2, 0, 0, true);
    case Id::VenomCoating:
        return MakeCard(id, u8"맹독 바르기", 1, u8"이번 전투 동안 공격마다 독 2.", CardType::Power, CardTargetType::None, CardArchetype::Poison, CardRarity::Uncommon, 2, 0, 0, 0, true, true);
    case Id::EvasiveStance:
        return MakeCard(id, u8"회피 자세", 1, u8"방어도 9.", CardType::Skill, CardTargetType::Self, CardArchetype::Poison, CardRarity::Common, 9, 0, 0, 0, true);
    case Id::CorrosiveMist:
        return MakeCard(id, u8"부식성 안개", 2, u8"독 5.", CardType::Skill, CardTargetType::Enemy, CardArchetype::Poison, CardRarity::Common, 5, 0, 0, 0, true);
    case Id::ToxicShield:
        return MakeCard(id, u8"독성 방패", 3, u8"방어도 획득 시 독 2.", CardType::Power, CardTargetType::None, CardArchetype::Poison, CardRarity::Rare, 2, 0, 0, 0, true, true);
    case Id::Plague:
        return MakeCard(id, u8"역병", 3, u8"적 독 2배. 1회용.", CardType::Skill, CardTargetType::Enemy, CardArchetype::Poison, CardRarity::Rare, 2, 0, 0, 0, true, true);
    case Id::Catalyst:
        return MakeCard(id, u8"촉매", 1, u8"적 독 1.5배.", CardType::Skill, CardTargetType::Enemy, CardArchetype::Poison, CardRarity::Uncommon, 150, 0, 0, 0, true);
    case Id::ToxicSkin:
        return MakeCard(id, u8"독성 피부", 1, u8"피격 시 독 8.", CardType::Power, CardTargetType::None, CardArchetype::Poison, CardRarity::Uncommon, 8, 0, 0, 0, false, true);
    case Id::SlowDeath:
        return MakeCard(id, u8"늦은 죽음", 1, u8"적 행동 시 독 피해 +2.", CardType::Power, CardTargetType::None, CardArchetype::Poison, CardRarity::Uncommon, 2, 0, 0, 0, false, true);
    case Id::VenomSlash:
        return MakeCard(id, u8"베놈 슬래시", 1, u8"독이 있으면 추가 피해.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Poison, CardRarity::Common, 5, 5);
    case Id::ParalysisPoison:
        return MakeCard(id, u8"마비독", 1, u8"독 3, 적 행위 시간 2초 지연.", CardType::Skill, CardTargetType::Enemy, CardArchetype::Poison, CardRarity::Uncommon, 3, 2);
    case Id::AcidDefense:
        return MakeCard(id, u8"산성 방어", 2, u8"독 6. 이미 독이 있으면 방어도 8.", CardType::Skill, CardTargetType::Enemy, CardArchetype::Poison, CardRarity::Uncommon, 6, 8);
    case Id::RefluxPoison:
        return MakeCard(id, u8"역류 독", 0, u8"독 2 제거 후 카드 2장 드로우.", CardType::Skill, CardTargetType::Enemy, CardArchetype::Poison, CardRarity::Common, 2, 2);

    case Id::FastHands:
        return MakeCard(id, u8"빠른 손놀림", 1, u8"카드 2장 드로우.", CardType::Skill, CardTargetType::Self, CardArchetype::Cycle, CardRarity::Common, 2, 0, 0, 0, true);
    case Id::Refit:
        return MakeCard(id, u8"재정비", 0, u8"버릴 때 카드 1장 드로우, 에너지 1.", CardType::Skill, CardTargetType::None, CardArchetype::Cycle, CardRarity::Common, 1, 1, 0, 0, true);
    case Id::Overcount:
        return MakeCard(id, u8"누적 과부하", 4, u8"이번 전투 드로우+버리기 횟수만큼 피해.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Cycle, CardRarity::Rare, 1, 0, 0, 0, true);
    case Id::TimeThief:
        return MakeCard(id, u8"시간 훔치기", 2, u8"적의 행위 주기 최대치를 늘립니다.", CardType::Skill, CardTargetType::Enemy, CardArchetype::Cycle, CardRarity::Uncommon, 2, 0, 0, 0, true);
    case Id::DeckDigging:
        return MakeCard(id, u8"덱 뒤지기", 2, u8"적 행위마다 성급 +1.", CardType::Power, CardTargetType::None, CardArchetype::Cycle, CardRarity::Rare, 1, 0, 0, 0, true, true);
    case Id::FullRedraw:
        return MakeCard(id, u8"손패 교체", 1, u8"손패를 모두 버리고 다시 뽑습니다.", CardType::Skill, CardTargetType::Self, CardArchetype::Cycle, CardRarity::Common, 0, 0, 0, 0, true);
    case Id::SwiftStrike:
        return MakeCard(id, u8"신속 타격", 0, u8"손에서 버리기 3회를 채워야 사용 가능합니다.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Cycle, CardRarity::Uncommon, 10, 1, 0, 0, true, false, 0, 3);
    case Id::ExplosiveStrike:
        return MakeCard(id, u8"폭발적 타격", 1, u8"이번 전투 동안 기본 타격 피해 +2.", CardType::Skill, CardTargetType::Self, CardArchetype::Cycle, CardRarity::Common, 2);
    case Id::ExplosiveDefend:
        return MakeCard(id, u8"폭발적 수비", 1, u8"이번 전투 동안 기본 수비 방어도 +1.", CardType::Skill, CardTargetType::Self, CardArchetype::Cycle, CardRarity::Common, 1);
    case Id::Recharge:
        return MakeCard(id, u8"충전", 0, u8"에너지를 모두 충전합니다. 1회용.", CardType::Skill, CardTargetType::Self, CardArchetype::Cycle, CardRarity::Rare, 0, 0, 0, 0, false, true);
    case Id::AccelCircuit:
        return MakeCard(id, u8"가속 회로", 1, u8"카드를 버릴 때 드로우 게이지 증가.", CardType::Power, CardTargetType::None, CardArchetype::Cycle, CardRarity::Uncommon, 35, 0, 0, 0, false, true);
    case Id::Overload:
        return MakeCard(id, u8"과부하", 5, u8"버릴 때 드로우와 에너지를 얻지만 피해 2배.", CardType::Power, CardTargetType::None, CardArchetype::Cycle, CardRarity::Rare, 1, 1, 0, 0, false, true);
    case Id::InstantFortify:
        return MakeCard(id, u8"찰나의 강화", 4, u8"손패 수에 비례한 방어도.", CardType::Skill, CardTargetType::Self, CardArchetype::Cycle, CardRarity::Uncommon, 2);
    case Id::ShredBurst:
        return MakeCard(id, u8"해체 충격", 3, u8"손패를 모두 버리고 버린 수만큼 타격.", CardType::Attack, CardTargetType::Enemy, CardArchetype::Cycle, CardRarity::Rare, 1);
    case Id::Blossom:
        return MakeCard(id, u8"개화", 4, u8"이번 전투 최대 에너지 +1.", CardType::Skill, CardTargetType::Self, CardArchetype::Cycle, CardRarity::Rare, 1);
    case Id::ChargeShield:
        return MakeCard(id, u8"충전 방어", 0, u8"남은 에너지를 모두 소비해 방어도를 얻습니다.", CardType::Skill, CardTargetType::Self, CardArchetype::Cycle, CardRarity::Uncommon, 5);
    case Id::HyperDraw:
        return MakeCard(id, u8"급가속", 2, u8"드로우 속도 증가, 최대 손패 -1.", CardType::Power, CardTargetType::None, CardArchetype::Cycle, CardRarity::Uncommon, 20, 1, 0, 0, false, true);
    case Id::SpeedAddiction:
        return MakeCard(id, u8"속도 중독", 2, u8"카드를 5장 버릴 때마다 모든 적에게 15 피해.", CardType::Power, CardTargetType::None, CardArchetype::Cycle, CardRarity::Rare, 15, 5, 0, 0, false, true);
    case Id::PatternRead:
        return MakeCard(id, u8"패턴 파악", 2, u8"적의 행위가 끝날 때마다 에너지 1.", CardType::Power, CardTargetType::None, CardArchetype::Cycle, CardRarity::Uncommon, 1, 0, 0, 0, false, true);
    default:
        return {};
    }
}

inline std::vector<CardData> BuildBaseStarterDeck() {
    return {
        CreateCardById(Id::Strike),
        CreateCardById(Id::Strike),
        CreateCardById(Id::Defend),
        CreateCardById(Id::Defend)
    };
}

inline std::vector<CardData> BuildStarterPackCards(CardArchetype archetype) {
    std::vector<int> ids;
    switch (archetype) {
    case CardArchetype::Combo:
        ids = { Id::ComboCore, Id::Jab, Id::RepeatingStab, Id::ComboRush, Id::ComboFinisher, Id::TimingSteal };
        break;
    case CardArchetype::Strength:
        ids = { Id::StrengthTraining, Id::CrimsonBreath, Id::HeavyStrike, Id::Endure, Id::LimitBreak, Id::BloodAccelerate };
        break;
    case CardArchetype::Block:
        ids = { Id::Condense, Id::ShieldBash, Id::Defend, Id::FirmPosture, Id::CompressedDefense, Id::EvasionMatrix };
        break;
    case CardArchetype::Poison:
        ids = { Id::PoisonSting, Id::VenomCoating, Id::EvasiveStance, Id::CorrosiveMist, Id::ToxicShield, Id::Plague, Id::Catalyst };
        break;
    case CardArchetype::Cycle:
        ids = { Id::FastHands, Id::Refit, Id::Overcount, Id::TimeThief, Id::DeckDigging, Id::FullRedraw, Id::SwiftStrike };
        break;
    default:
        break;
    }

    std::vector<CardData> cards;
    cards.reserve(ids.size());
    for (int id : ids) {
        cards.push_back(CreateCardById(id));
    }
    return cards;
}

inline std::vector<CardData> BuildGeneralCardPool() {
    const std::vector<int> ids = {
        Id::Strike, Id::Defend,
        Id::BouncingBlade, Id::ExploitOpening, Id::PressureBreathing, Id::ComboStance, Id::ResetClock, Id::FrenzyCuts, Id::WhirlBeat,
        Id::Stockpile, Id::GiantArm, Id::Warmup, Id::Crush, Id::BattleInstinct, Id::BerserkFlow, Id::LeechBlade,
        Id::Invincible, Id::ReactiveGuard, Id::WeightLoad, Id::CounterStance, Id::ShieldSpin, Id::GuardConversion, Id::WallCollapse,
        Id::ToxicSkin, Id::SlowDeath, Id::VenomSlash, Id::ParalysisPoison, Id::AcidDefense, Id::RefluxPoison,
        Id::ExplosiveStrike, Id::ExplosiveDefend, Id::Recharge, Id::AccelCircuit, Id::Overload, Id::InstantFortify, Id::ShredBurst, Id::Blossom, Id::ChargeShield, Id::HyperDraw, Id::SpeedAddiction, Id::PatternRead
    };

    std::vector<CardData> cards;
    cards.reserve(ids.size());
    for (int id : ids) {
        cards.push_back(CreateCardById(id));
    }
    return cards;
}

inline std::vector<CardData> BuildArchetypeRewardPool(CardArchetype archetype) {
    std::vector<CardData> cards;
    for (const CardData& card : BuildGeneralCardPool()) {
        if (card.archetype == archetype) {
            cards.push_back(card);
        }
    }
    return cards;
}

} // namespace CardLibrary
