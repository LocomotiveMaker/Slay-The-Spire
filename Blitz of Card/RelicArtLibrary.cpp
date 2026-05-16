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
    static const std::vector<std::string> kBloodstone = {
        " /\\  ",
        "<##> ",
        " \\/  "
    };
    static const std::vector<std::string> kShieldGear = {
        " /O\\ ",
        "|###|",
        " \\_/ "
    };
    static const std::vector<std::string> kAlchemyPouch = {
        " /\\  ",
        "(~~) ",
        " \\/  "
    };
    static const std::vector<std::string> kGoldenTooth = {
        " /\\  ",
        "|$$| ",
        " \\/  "
    };
    static const std::vector<std::string> kCrackedCharm = {
        " .*. ",
        "<\\/ >",
        " `*' "
    };

    switch (artId) {
    case RelicArtId::Bloodstone: return kBloodstone;
    case RelicArtId::ShieldGear: return kShieldGear;
    case RelicArtId::AlchemyPouch: return kAlchemyPouch;
    case RelicArtId::GoldenTooth: return kGoldenTooth;
    case RelicArtId::CrackedCharm: return kCrackedCharm;
    default: return kFallback;
    }
}

const std::vector<std::string>& Get(const RelicData& relic) {
    return Get(static_cast<RelicArtId>(relic.artId));
}

} // namespace RelicArtLibrary
