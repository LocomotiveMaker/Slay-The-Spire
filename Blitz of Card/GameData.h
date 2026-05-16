// -----------------------------------------------------------------------------
// @file       GameData.h
// @brief      프론트엔드와 백엔드를 연결하는 데이터 인터페이스
// @note       UI 요소들은 이 구조체의 포인터를 참조하여 상태를 렌더링함.
// -----------------------------------------------------------------------------
#pragma once
#include <string>
#include <vector>

// 카드 종류 열거형
enum class CardType { Attack, Skill, Power, Status, Curse };
enum class CardTargetType { None, Enemy, Self };
enum class CardEffectType {
    None,
    AttackDamage,
    DefendBlock,
    ApplyVulnerable,
    Custom
};
enum class CardDiscardEffectType {
    None,
    DrawCardsGainEnergy,
    Custom
};
enum class CardRarity {
    Common,
    Uncommon,
    Rare
};
enum class CardArchetype {
    None,
    Basic,
    Combo,
    Strength,
    Block,
    Poison,
    Cycle
};

enum class RelicArtId {
    None,
    Bloodstone,
    ShieldGear,
    AlchemyPouch,
    GoldenTooth,
    CrackedCharm
};

// 카드 데이터 구조체
struct CardData {
    int id = 0;
    std::string baseName;
    std::string name;
    int cost = 0;
    std::string baseDescription;
    std::string description;
    CardType type = CardType::Skill;
    CardTargetType targetType = CardTargetType::None;
    CardEffectType effectType = CardEffectType::None;
    CardDiscardEffectType discardEffectType = CardDiscardEffectType::None;
    CardRarity rarity = CardRarity::Common;
    CardArchetype archetype = CardArchetype::None;
    bool starterPackOnly = false;
    bool exhausts = false;
    bool uniquePower = false;
    int primaryValue = 0;
    int secondaryValue = 0;
    int tertiaryValue = 0;
    int quaternaryValue = 0;
    int upgradeLevel = 0;
    int baseReuseCount = 0;
    int remainingReuseCount = 0;
    int discardRequirement = 0;
    int discardProgress = 0;
    int timesUsedThisBattle = 0;
    int runtimePrimaryModifier = 0;
    int runtimeSecondaryModifier = 0;
};

// 전투 개체(플레이어, 몬스터) 데이터 구조체
struct EntityData {
    int id = 0;
    int visualArtId = 0;
    std::string name;
    int currentHp = 0;
    int maxHp = 0;
    int block = 0;
    int strength = 0;
    int dexterity = 0;
    int vulnerable = 0;
    int weak = 0;
    int poison = 0;
};

struct RelicData {
    int id = 0;
    int artId = 0;
    std::string name;
    std::string description;
};

struct PotionData {
    int id = 0;
    std::string name;
    std::string description;
    bool battleOnly = false;
};
