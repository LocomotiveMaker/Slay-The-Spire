// -----------------------------------------------------------------------------
// @file       AsciiArtLibrary.h
// @brief      수정 가능한 UTF-8 아스키/유니코드 아트 등록 지점
//             팀원은 우선 이 파일과 cpp의 아트 블록부터 수정하면 된다.
// -----------------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>

enum class AsciiArtId {
    TitleLogo,
    PlayerBattle,
    PlayerDeath,
    PlayerCardPack,
    EnemyNormal,
    EnemyElite,
    EnemyBoss,
    Neow
};

namespace AsciiArtLibrary {

// Returns the normalized art block for the requested scene/entity role.
const std::vector<std::string>& Get(AsciiArtId id);

// Trims common indentation and blank margins so pasted art is easier to manage.
std::vector<std::string> Normalize(const std::vector<std::string>& lines);

// Art-safe path:
// removes only shared source indentation, preserves blank rows/trailing blanks,
// and canonicalizes blank-like glyphs to ASCII space for stable console output.
std::vector<std::string> PreserveArtLayout(const std::vector<std::string>& lines);

} // namespace AsciiArtLibrary
