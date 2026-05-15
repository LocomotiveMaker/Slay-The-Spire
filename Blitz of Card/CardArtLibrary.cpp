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
        u8"⠀⠀⢻⣦⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠹⣷⡄⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠈⢿⣦⠀⢀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⢻⣿⡃⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠁⠀⠙⣦⠀⠀"
    };

    static const std::vector<std::string> kDefend = {
        u8"⠀⠀⣶⣾⣿⣿⣷⣶⠀⠀",
        u8"⠀⠀⣿⣿⣿⣿⣿⣿⠀⠀",
        u8"⠀⠀⢹⣿⣿⣿⣿⡟⠀⠀",
        u8"⠀⠀⠀⠻⣿⣿⠟⠀⠀⠀",
        u8"⠀⠀⠀⠀⠈⠁⠀⠀⠀"
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
        u8"⠀⠀⣴⡄⠀⣀⣀⠀⢠⣦⠀⠀",
        u8"⠀⠘⠛⢷⣾⣿⣿⣷⡾⠛⠃⠀",
        u8"⠀⠀⠀⢘⡛⢻⡟⢋⡇⠀⠀⠀",
        u8"⠀⢠⣤⡾⠛⣿⣾⠛⢷⣤⡄⠀",
        u8"⠀⠀⠻⠃⠀⠈⠁⠀⠘⠟⠀⠀"
    };

    static const std::vector<std::string> kHealing = {
        u8"⠀⠀⠀⠀⣤⣤⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⣿⣿⠀⠀⠀⠀",
        u8"⠀⣶⣶⣶⣿⣿⣶⣶⣶⠀",
        u8"⠀⠿⠿⠿⣿⣿⠿⠿⠿⠀",
        u8"⠀⠀⠀⠀⣿⣿⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠛⠛⠀⠀⠀⠀"
    };

    static const std::vector<std::string> kDraw = {
        u8"⠀⢀⣴⡿⠟⠻⢿⣶⠀⠀",
        u8"⠀⣼⡟⠀⠀⠀⠈⠉⠀⠀",
        u8"⠀⢀⡀⠀⠀⠀⠀⣼⡇⠀",
        u8"⠀⠘⢿⣧⣤⣤⣾⠟⠁⠀",
        u8"⠀⠀⠀⠈⠉⠉⠁⠀⠀⠀"
    };

    static const std::vector<std::string> kStrength = {
        u8"⠀⠀⠀⠀⠀⢀⣠⣤⡀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⢀⣾⠛⠛⠁⠀⠀⠀⠀",
        u8"⠀⠀⠀⢀⣾⣿⠀⣀⣀⠀⠀⠀⠀",
        u8"⠀⠀⠀⢸⣿⣿⣾⣿⣿⣧⠀⠀⠀",
        u8"⠀⠀⠀⠸⣿⣿⣿⣿⣿⠏⠀"
    };

    static const std::vector<std::string> kCharge = {
        u8"⠀⠀⢀⠤⠤⠤⠤⠤⠤⠤⠀⠀⠀",
        u8"⠀⠀⢸⢸⣿⢸⣿⢸⣿⡇⣷⠀⠀",
        u8"⠀⠀⢸⠸⠿⠸⠿⠸⠿⠇⠟⠀⠀",
        u8"⠀⠀⠈⠉⠉⠉⠉⠉⠉⠉⠀⠀⠀"
    };

    static const std::vector<std::string> kCombo = {
        u8"⠀⠀⠀⠀⢀⣼⢿⣆⠀⠀⠀⠀",
        u8"⠀⠀⢀⣴⠟⠁⠀⠙⢷⣄⠀⠀",
        u8"⠀⠀⠈⠉⠀⢠⣤⠀⠀⠉⠀⠀",
        u8"⠀⠀⠀⢀⣴⠟⠙⢷⣄⠀⠀⠀",
        u8"⠀⠀⠠⡿⠁⠀⠀⠀⠙⠧⠀⠀"
    };

    static const std::vector<std::string> kBlock = {
        u8"⠀⠀⠀⠀⣴⣾⣷⣦⠀⠀⠀⠀",
        u8"⠀⠀⠀⢸⣿⣿⣿⣿⡇⠀⠀⠀",
        u8"⠀⠀⠀⢸⣭⣹⣉⣭⡇⠀⠀⠀",
        u8"⠀⠀⠀⠸⣿⣿⣿⣿⠇⠀⠀⠀",
        u8"⠀⠀⠀⠀⠈⠻⠟⠁⠀⠀⠀⠀"
    };

    static const std::vector<std::string> kTime = {
        u8"⠀⠀⠀⣠⠔⠒⠒⠢⣄⠀⠀⠀",
        u8"⠀⠀⢰⠁⠀⠀⡆⠀⠈⢆⠀⠀",
        u8"⠀⠀⣇⠀⠀⠘⢧⡀⠀⢸⠀⠀",
        u8"⠀⠀⠸⣄⠀⠀⠀⠉⣠⠇⠀⠀",
        u8"⠀⠀⠀⠈⠓⠒⠒⠚⠁⠀⠀⠀"
    };

    static const std::vector<std::string> kPowerAura = {
        u8"⠀⠀⠀⠀⠀⠀⡀⠀⠀⠀⠀⠀",
        u8"⠀⠀⢀⠈⠀⢸⡇⠀⢀⡄⠀⠀",
        u8"⠀⠀⠀⢳⣾⡞⢳⣷⡟⢀⠀⠀",
        u8"⠀⠀⢁⣠⡧⣤⣦⢸⣖⡠⠀⠀",
        u8"⠀⠀⠀⠙⠂⠙⠋⠐⠃⠀⠀⠀"
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
    case CardLibrary::Id::LeechBlade: return kHealing;
    case CardLibrary::Id::FastHands:
    case CardLibrary::Id::Refit:
    case CardLibrary::Id::Overcount:
    case CardLibrary::Id::DeckDigging:
    case CardLibrary::Id::FullRedraw:
    case CardLibrary::Id::SwiftStrike:
    case CardLibrary::Id::AccelCircuit:
    case CardLibrary::Id::Overload:
    case CardLibrary::Id::ShredBurst:
    case CardLibrary::Id::HyperDraw:
    case CardLibrary::Id::SpeedAddiction:
        return kDraw;
    case CardLibrary::Id::Recharge:
    case CardLibrary::Id::ChargeShield:
    case CardLibrary::Id::Stockpile:
    case CardLibrary::Id::Blossom:
        return kCharge;
    case CardLibrary::Id::TimingSteal:
    case CardLibrary::Id::ResetClock:
    case CardLibrary::Id::TimeThief:
    case CardLibrary::Id::ParalysisPoison:
    case CardLibrary::Id::PatternRead:
        return kTime;
    case CardLibrary::Id::ComboCore:
    case CardLibrary::Id::Jab:
    case CardLibrary::Id::RepeatingStab:
    case CardLibrary::Id::ComboRush:
    case CardLibrary::Id::ComboFinisher:
    case CardLibrary::Id::BouncingBlade:
    case CardLibrary::Id::ExploitOpening:
    case CardLibrary::Id::PressureBreathing:
    case CardLibrary::Id::ComboStance:
    case CardLibrary::Id::FrenzyCuts:
    case CardLibrary::Id::WhirlBeat:
        return kCombo;
    case CardLibrary::Id::Condense:
    case CardLibrary::Id::ShieldBash:
    case CardLibrary::Id::FirmPosture:
    case CardLibrary::Id::CompressedDefense:
    case CardLibrary::Id::EvasionMatrix:
    case CardLibrary::Id::Invincible:
    case CardLibrary::Id::ReactiveGuard:
    case CardLibrary::Id::WeightLoad:
    case CardLibrary::Id::CounterStance:
    case CardLibrary::Id::ShieldSpin:
    case CardLibrary::Id::GuardConversion:
    case CardLibrary::Id::WallCollapse:
    case CardLibrary::Id::InstantFortify:
    case CardLibrary::Id::EvasiveStance:
        return kBlock;
    case CardLibrary::Id::ExplosiveStrike:
    case CardLibrary::Id::ExplosiveDefend:
        return kPowerAura;
    default:
        break;
    }

    if (card.archetype == CardArchetype::Poison) {
        return kPoison;
    }
    if (card.archetype == CardArchetype::Strength) {
        return kStrength;
    }

    switch (card.type) {
    case CardType::Attack: return kAttackDefault;
    case CardType::Skill:  return kSkillDefault;
    case CardType::Power:  return kPowerAura;
    default:               return kSkillDefault;
    }
}

} // namespace CardArtLibrary
