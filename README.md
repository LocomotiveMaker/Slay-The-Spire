# Slay The Spire

Windows console ASCII prototype inspired by Slay the Spire.

## GitHub share links

- Repository: https://github.com/LocomotiveMaker/Slay-The-Spire
- Main branch: https://github.com/LocomotiveMaker/Slay-The-Spire/tree/main
- Example file link:
  https://github.com/LocomotiveMaker/Slay-The-Spire/blob/main/Slay%20The%20Spire/AsciiArtLibrary.cpp

## Team edit points

- Combat / card-pack / death / Neow art:
  [AsciiArtLibrary.cpp](/C:/github/Slay%20The%20Spire/Slay%20The%20Spire/AsciiArtLibrary.cpp)
- Art ids and entry points:
  [AsciiArtLibrary.h](/C:/github/Slay%20The%20Spire/Slay%20The%20Spire/AsciiArtLibrary.h)
- Enemy art routing by id:
  [EntityUI.cpp](/C:/github/Slay%20The%20Spire/Slay%20The%20Spire/EntityUI.cpp)
- Card-pack scene layout / clip / Neow placement:
  [main.cpp](/C:/github/Slay%20The%20Spire/Slay%20The%20Spire/main.cpp)

## Build

- Visual Studio 2022 is required with `.vsconfig` components installed.
- Required components are C++ desktop tools and Windows 10 SDK 10.0.19041.0 or newer.
- If Visual Studio reports missing external dependency headers such as `windows.h`, `winnt.h`, or `winuser.h`, open Visual Studio Installer and import `.vsconfig` from the repository root.
- Open `Slay The Spire.sln` in Visual Studio.
- Build `Debug | x64` or `Release | x64`.
- Run the generated `Slay The Spire.exe`.
- Command-line build:
  `powershell -ExecutionPolicy Bypass -File tools/Build-Solution.ps1 -Configuration Release -Platform x64`

## Notes

- For most art replacements, teammates only need to edit string arrays in `AsciiArtLibrary.cpp`.
- `Normalize(...)` trims shared indentation, so art can be pasted with leading spaces.
- Current enemy art routing is:
  - `id >= 9200` -> boss art
  - `id >= 9100` -> elite art
  - otherwise -> normal enemy art
- If layout needs to change, adjust render positions in `main.cpp`, not the art strings first.
