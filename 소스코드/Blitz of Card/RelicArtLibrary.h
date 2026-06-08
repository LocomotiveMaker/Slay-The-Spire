// -----------------------------------------------------------------------------
// @file       RelicArtLibrary.h
// @brief      Small relic icons used by the relic drawer panel.
// -----------------------------------------------------------------------------
#pragma once

#include "GameData.h"

#include <string>
#include <vector>

namespace RelicArtLibrary {

const std::vector<std::string>& Get(RelicArtId artId);
const std::vector<std::string>& Get(const RelicData& relic);

} // namespace RelicArtLibrary
