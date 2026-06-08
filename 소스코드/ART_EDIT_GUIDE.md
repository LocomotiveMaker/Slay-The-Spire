# ASCII Art Edit Guide

## 1. Main edit file

Edit art in:
[AsciiArtLibrary.cpp](/C:/Slay-The-Spire/Blitz%20of%20Card/AsciiArtLibrary.cpp)

This file currently owns:

- player battle art
- player death art
- player card-pack art
- normal / elite / boss enemy art
- Neow art

## 2. How to replace art

1. Find the matching `k...` block in `AsciiArtLibrary.cpp`.
2. Replace only the string rows inside `Normalize({ ... })`.
3. Keep the block name the same unless code routing also changes.

## 3. Where art is used

- Combat entities:
  [EntityUI.cpp](/C:/Slay-The-Spire/Blitz%20of%20Card/EntityUI.cpp)
- Card-pack scene / Neow clip:
  [main.cpp](/C:/Slay-The-Spire/Blitz%20of%20Card/main.cpp)

## 4. Routing rules

Current combat enemy art selection is id-based:

- `id >= 9200`: boss art
- `id >= 9100`: elite art
- else: normal enemy art

If a teammate wants per-enemy art later, the safest next step is to extend
`ResolveEntityArt(...)` in `EntityUI.cpp`.

## 5. Important practical rule

If the art looks wrong on screen, check layout and clip first.
Do not immediately distort the source art to fit a bad pivot or bad clip.
