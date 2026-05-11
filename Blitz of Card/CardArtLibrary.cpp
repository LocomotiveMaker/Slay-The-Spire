// -----------------------------------------------------------------------------
// @file       CardArtLibrary.cpp
// @brief      Card-local mini art used inside CardUI.
// -----------------------------------------------------------------------------
#include "CardArtLibrary.h"

#include "CardLibrary.h"

namespace CardArtLibrary {

const std::vector<std::string>& Get(const CardData& card) {
    // Team edit point:
    // add per-card art here and map it in the switch below.
    // Any card without its own case falls back to a type-based default.
    static const std::vector<std::string> kAttackDefault = {
        u8"  /\\  /\\\\  ",
        u8" // \\\\// \\\\ ",
        u8" \\\\  /\\\\  / ",
        u8"  \\\\//  \\\\/  ",
        u8"    STRIKE    "
    };

    static const std::vector<std::string> kSkillDefault = {
        u8"   ________   ",
        u8"  / ____  /|  ",
        u8" / /___/ / /  ",
        u8"/_______/ /   ",
        u8"|_______|/    "
    };

    static const std::vector<std::string> kPowerDefault = {
        u8"   .-====-.   ",
        u8"  /  /\\   \\\\  ",
        u8" |  |  |  | | ",
        u8"  \\\\  \\/  //  ",
        u8"   `-====-'   "
    };

    static const std::vector<std::string> kStrike = {
        u8"     /\\       ",
        u8"    /  \\\\     ",
        u8"---<====>---  ",
        u8"    \\\\  //    ",
        u8"     \\/       "
    };

    static const std::vector<std::string> kDefend = {
        u8"    ____      ",
        u8"   / __ \\\\     ",
        u8"  / /  \\\\ \\\\    ",
        u8"  \\\\ \\\\__/ /    ",
        u8"   \\\\____/     "
    };

    static const std::vector<std::string> kComboCore = {
        u8"   <>  <>     ",
        u8"  <  ><  >    ",
        u8"   ><  ><     ",
        u8"  <  ><  >    ",
        u8"   <>  <>     "
    };

    static const std::vector<std::string> kTimingSteal = {
        u8"   .----.     ",
        u8"  / 12  \\\\    ",
        u8" | 9  o 3 |   ",
        u8"  \\\\  6  //    ",
        u8"   `----'     "
    };

    static const std::vector<std::string> kCondense = {
        u8"  [########]  ",
        u8"  [########]  ",
        u8"  [########]  ",
        u8"  [########]  ",
        u8"  [########]  "
    };

    static const std::vector<std::string> kPoison = {
        u8"    .--.      ",
        u8"   / __ \\\\     ",
        u8"  / /  \\\\ \\\\    ",
        u8"  \\\\ \\\\__/ /    ",
        u8"   `----'     "
    };

    static const std::vector<std::string> kCycle = {
        u8"   .----.     ",
        u8"  / .--. \\\\    ",
        u8" | |    | |   ",
        u8"  \\\\ '--' /    ",
        u8"   `----'     "
    };

    switch (card.id) {
    case CardLibrary::Id::Strike: return kStrike;
    case CardLibrary::Id::Defend: return kDefend;
    case CardLibrary::Id::ComboCore: return kComboCore;
    case CardLibrary::Id::TimingSteal: return kTimingSteal;
    case CardLibrary::Id::Condense: return kCondense;
    default:
        break;
    }

    switch (card.type) {
    case CardType::Attack: return kAttackDefault;
    case CardType::Skill:  return (card.archetype == CardArchetype::Poison) ? kPoison : kSkillDefault;
    case CardType::Power:  return (card.archetype == CardArchetype::Cycle) ? kCycle : kPowerDefault;
    default:               return kSkillDefault;
    }
}

} // namespace CardArtLibrary
