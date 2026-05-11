# Blitz of Card

Windows console ASCII card-battle prototype.

## GitHub share links

- Repository URL is configured in `git remote -v`.
- Main branch source is under `Blitz of Card/`.

## Team edit points

- Combat / card-pack / death / Neow art:
  [AsciiArtLibrary.cpp](/C:/Slay-The-Spire/Blitz%20of%20Card/AsciiArtLibrary.cpp)
- Art ids and entry points:
  [AsciiArtLibrary.h](/C:/Slay-The-Spire/Blitz%20of%20Card/AsciiArtLibrary.h)
- Enemy art routing by id:
  [EntityUI.cpp](/C:/Slay-The-Spire/Blitz%20of%20Card/EntityUI.cpp)
- Card-pack scene layout / clip / Neow placement:
  [main.cpp](/C:/Slay-The-Spire/Blitz%20of%20Card/main.cpp)

## Build

- Visual Studio 2022 is required with `.vsconfig` components installed.
- Required components are C++ desktop tools and Windows 10 SDK 10.0.19041.0 or newer.
- If Visual Studio reports missing external dependency headers such as `windows.h`, `winnt.h`, or `winuser.h`, open Visual Studio Installer and import `.vsconfig` from the repository root.
- Open `Blitz of Card.sln` in Visual Studio.
- Build `Debug | x64` or `Release | x64`.
- Run the generated `Blitz of Card.exe`.
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
