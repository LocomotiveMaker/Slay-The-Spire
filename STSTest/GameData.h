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

// 카드 데이터 구조체
struct CardData {
    int id;
    std::string name;
    int cost;
    std::string description;
    CardType type;
};

// 전투 개체(플레이어, 몬스터) 데이터 구조체
struct EntityData {
    int id;
    std::string name;
    int currentHp;
    int maxHp;
    int block;
    // 추후 버프/디버프 배열 추가 예정
};