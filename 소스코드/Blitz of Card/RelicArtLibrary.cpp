// -----------------------------------------------------------------------------
// @file       RelicArtLibrary.cpp
// @brief      Team-editable relic mini-art registry.
// -----------------------------------------------------------------------------
#include "RelicArtLibrary.h"

namespace RelicArtLibrary {

const std::vector<std::string>& Get(RelicArtId artId) {
    // Team edit point:
    // keep relic icons compact. The panel currently expects roughly 5x3 cells.
    static const std::vector<std::string> kFallback = {
        " .-. ",
        "( ? )",
        " '-' "
    };
    static const std::vector<std::string> kOldClock = { " .-. ", "(o o)", " `-' " };
    static const std::vector<std::string> kSharpMetronome = { " /|\\ ", "/_|_\\", "  |  " };
    static const std::vector<std::string> kCrackedShield = { " /#\\ ", "|/\\|", " \\_/ " };
    static const std::vector<std::string> kPoisonNeedle = { "  /\\ ", "===> ", "  \\/ " };
    static const std::vector<std::string> kRedMedal = { " \\|/ ", "(*) ", " / \\ " };
    static const std::vector<std::string> kSwiftWristband = { "==== ", "{~~}", "==== " };
    static const std::vector<std::string> kPreservedEmber = { " (  ", "(**)", " /\\ " };
    static const std::vector<std::string> kMerchantCoin = { " ___ ", "($$)", " --- " };
    static const std::vector<std::string> kCrackedHourglass = { "\\ / ", " X  ", "/ \\" };
    static const std::vector<std::string> kBattleFeather = { " // ", "<<  ", " \\\\ " };
    static const std::vector<std::string> kBronzeHeart = { "/\\/\\", "\\  /", " \\/ " };
    static const std::vector<std::string> kSharpCharm = { " .*. ", "<\\/ >", " `*' " };

    switch (artId) {
    case RelicArtId::OldClock: return kOldClock;
    case RelicArtId::SharpMetronome: return kSharpMetronome;
    case RelicArtId::CrackedShield: return kCrackedShield;
    case RelicArtId::PoisonNeedle: return kPoisonNeedle;
    case RelicArtId::RedMedal: return kRedMedal;
    case RelicArtId::SwiftWristband: return kSwiftWristband;
    case RelicArtId::PreservedEmber: return kPreservedEmber;
    case RelicArtId::MerchantCoin: return kMerchantCoin;
    case RelicArtId::CrackedHourglass: return kCrackedHourglass;
    case RelicArtId::BattleFeather: return kBattleFeather;
    case RelicArtId::BronzeHeart: return kBronzeHeart;
    case RelicArtId::SharpCharm: return kSharpCharm;
    default: return kFallback;
    }
}

const std::vector<std::string>& Get(const RelicData& relic) {
    return Get(static_cast<RelicArtId>(relic.artId));
}

} // namespace RelicArtLibrary
