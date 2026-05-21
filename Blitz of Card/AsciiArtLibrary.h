// -----------------------------------------------------------------------------
// @file       AsciiArtLibrary.h
// @brief      수정 가능한 UTF-8 아스키/유니코드 아트 등록 지점
//             팀원은 우선 이 파일과 cpp의 아트 블록부터 수정하면 된다.
// -----------------------------------------------------------------------------
#pragma once

#include "GameData.h"

#include <string>
#include <vector>

enum class AsciiArtId {
    TitleLogo,
    PlayerBattle,
    PlayerBattleCombo,
    PlayerBattleStrength,
    PlayerBattleBlock,
    PlayerBattlePoison,
    PlayerBattleCycle,
    PlayerAttackCombo,
    PlayerAttackStrength,
    PlayerAttackBlock,
    PlayerAttackPoison,
    PlayerAttackCycle,
    PlayerDefendCombo,
    PlayerDefendStrength,
    PlayerDefendBlock,
    PlayerDefendPoison,
    PlayerDefendCycle,
    PlayerDeath,
    PlayerDeathCombo,
    PlayerDeathStrength,
    PlayerDeathBlock,
    PlayerDeathPoison,
    PlayerDeathCycle,
    PlayerCardPack,
    EnemyNormal,
    EnemyNormalGoblin,
    EnemyNormalSkeleton,
    EnemyNormalGolem,
    EnemyNormalBat,
    EnemyNormalMushroom,
    EnemyNormalSlime,
    EnemyElite,
    EnemyEliteWitch,
    EnemyEliteReaper,
    EnemyEliteBackturner,
    EnemyBoss,
    EnemyBossCentaurus,
    EnemyBossPuppet,
    EnemyBossHydra,
    EnemyBossTitan,
    EnemyBossCrown,
    Neow,
    Merchant,
    TreasureChestClosed,
    TreasureChestOpen,
    Campfire
};

namespace AsciiArtLibrary {

// Returns the normalized art block for the requested scene/entity role.
const std::vector<std::string>& Get(AsciiArtId id);

// Returns the player battle art that matches the selected starter deck archetype.
const std::vector<std::string>& GetPlayerBattle(CardArchetype archetype);

// Returns the player attack pose that matches the selected starter deck archetype.
const std::vector<std::string>& GetPlayerAttack(CardArchetype archetype);

// Returns the player defensive/hit pose that matches the selected starter deck archetype.
const std::vector<std::string>& GetPlayerDefend(CardArchetype archetype);

// Returns the player death pose that matches the selected starter deck archetype.
const std::vector<std::string>& GetPlayerDeath(CardArchetype archetype);

// Returns one of the large boss monster art variants for a newly spawned boss.
const std::vector<std::string>& GetRandomEnemyBoss();

// Returns one of the normal monster art variants for a newly spawned monster.
const std::vector<std::string>& GetRandomEnemyNormal();

// Trims common indentation and blank margins so pasted art is easier to manage.
std::vector<std::string> Normalize(const std::vector<std::string>& lines);

// Art-safe path:
// removes only shared source indentation, preserves blank rows/trailing blanks,
// and canonicalizes blank-like glyphs to ASCII space for stable console output.
std::vector<std::string> PreserveArtLayout(const std::vector<std::string>& lines);

} // namespace AsciiArtLibrary
