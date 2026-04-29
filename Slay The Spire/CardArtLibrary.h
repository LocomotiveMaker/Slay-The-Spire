// -----------------------------------------------------------------------------
// @file       CardArtLibrary.h
// @brief      Central card art registry. Extend this file first when a specific
//             card needs a custom mini-art inside the card frame.
// -----------------------------------------------------------------------------
#pragma once

#include "GameData.h"

#include <string>
#include <vector>

namespace CardArtLibrary {

const std::vector<std::string>& Get(const CardData& card);

} // namespace CardArtLibrary
