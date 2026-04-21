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
enum class CardEffectType { None, AttackDamage, DefendBlock, ApplyVulnerable };
enum class CardDiscardEffectType { None, DrawCardsGainEnergy };

// 카드 데이터 구조체
struct CardData {
    int id;
    std::string name;
    int cost;
    std::string description;
    CardType type;
    CardTargetType targetType = CardTargetType::None;
    CardEffectType effectType = CardEffectType::None;
    CardDiscardEffectType discardEffectType = CardDiscardEffectType::None;
    int primaryValue = 0;
    int secondaryValue = 0;
};

// 전투 개체(플레이어, 몬스터) 데이터 구조체
struct EntityData {
    int id;
    std::string name;
    int currentHp;
    int maxHp;
    int block;
    int strength = 0;
    int vulnerable = 0;
    int weak = 0;
    int poison = 0;
};
