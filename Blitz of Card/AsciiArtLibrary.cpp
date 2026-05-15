// -----------------------------------------------------------------------------
// @file       AsciiArtLibrary.cpp
// @brief      씬/엔티티별 아스키 아트 원본 보관소
// -----------------------------------------------------------------------------
#include "AsciiArtLibrary.h"

#include "TextLayout.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <windows.h>

namespace AsciiArtLibrary {
namespace {

bool IsBlankLike(wchar_t ch) {
    return ch == L' ' || ch == 0x2800 || ch == L'\t';
}

std::wstring CanonicalizeArtWhitespace(std::wstring wideLine) {
    for (wchar_t& ch : wideLine) {
        if (IsBlankLike(ch)) {
            ch = L' ';
        }
    }
    return wideLine;
}

std::string WideToUtf8(const std::wstring& wideText) {
    if (wideText.empty()) {
        return {};
    }

    const int utf8Length = WideCharToMultiByte(
        CP_UTF8,
        0,
        wideText.data(),
        static_cast<int>(wideText.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (utf8Length <= 0) {
        return "?";
    }

    std::string utf8Text(static_cast<size_t>(utf8Length), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wideText.data(),
        static_cast<int>(wideText.size()),
        utf8Text.data(),
        utf8Length,
        nullptr,
        nullptr);
    return utf8Text;
}

bool HasPrefix(const std::string& text, const std::string& prefix) {
    return text.size() >= prefix.size() &&
        std::equal(prefix.begin(), prefix.end(), text.begin());
}

bool IsBlankLikeLine(const std::wstring& line) {
    return std::all_of(line.begin(), line.end(), [](wchar_t ch) {
        return IsBlankLike(ch);
    });
}

std::vector<std::string> TrimBlankArtMargins(const std::vector<std::string>& lines) {
    size_t firstContent = 0;
    while (firstContent < lines.size() &&
        IsBlankLikeLine(TextLayout::Utf8ToWide(lines[firstContent]))) {
        ++firstContent;
    }

    size_t lastContent = lines.size();
    while (lastContent > firstContent &&
        IsBlankLikeLine(TextLayout::Utf8ToWide(lines[lastContent - 1]))) {
        --lastContent;
    }

    return std::vector<std::string>(lines.begin() + firstContent, lines.begin() + lastContent);
}

bool IsNormalMonsterHeading(const std::string& line) {
    return HasPrefix(line, u8"고블린") ||
        HasPrefix(line, u8"스켈레톤") ||
        HasPrefix(line, u8"골렘") ||
        HasPrefix(line, u8"박쥐") ||
        HasPrefix(line, u8"머쉬룸") ||
        HasPrefix(line, u8"슬라임");
}

bool TryReadArtSectionHeading(
    const std::string& line,
    std::string& heading,
    std::string& remainder) {
    static const std::vector<std::wstring> kSectionHeadings = {
        L"고블린",
        L"스켈레톤",
        L"골렘",
        L"박쥐",
        L"머쉬룸",
        L"슬라임",
        L"스탠딩",
        L"방어",
        L"공격",
        L"피격",
        L"사망",
        L"죽음"
    };

    const std::wstring wideLine = TextLayout::Utf8ToWide(line);
    size_t contentStart = 0;
    while (contentStart < wideLine.size() && IsBlankLike(wideLine[contentStart])) {
        ++contentStart;
    }

    const std::wstring content = wideLine.substr(contentStart);
    for (const std::wstring& candidate : kSectionHeadings) {
        if (content.size() < candidate.size()) {
            continue;
        }

        if (!std::equal(candidate.begin(), candidate.end(), content.begin())) {
            continue;
        }

        heading = WideToUtf8(candidate);
        remainder = WideToUtf8(content.substr(candidate.size()));
        return true;
    }

    return false;
}

std::vector<std::string> LoadPreservedArtFile(
    const std::filesystem::path& relativePath,
    const std::vector<std::string>& fallback) {
    const std::filesystem::path sourceRootPath = std::filesystem::path("Blitz of Card") / relativePath;
    const std::filesystem::path parentPath = std::filesystem::path("..") / relativePath;
    const std::filesystem::path candidates[] = {
        relativePath,
        sourceRootPath,
        parentPath
    };

    for (const std::filesystem::path& path : candidates) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            continue;
        }

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(line);
        }

        if (!lines.empty()) {
            return PreserveArtLayout(TrimBlankArtMargins(lines));
        }
    }

    return fallback;
}

std::vector<std::string> LoadPreservedArtSection(
    const std::filesystem::path& relativePath,
    const std::string& sectionName,
    const std::vector<std::string>& fallback) {
    const std::filesystem::path sourceRootPath = std::filesystem::path("Blitz of Card") / relativePath;
    const std::filesystem::path parentPath = std::filesystem::path("..") / relativePath;
    const std::filesystem::path candidates[] = {
        relativePath,
        sourceRootPath,
        parentPath
    };

    for (const std::filesystem::path& path : candidates) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            continue;
        }

        std::vector<std::string> lines;
        std::string line;
        bool collecting = false;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            std::string heading;
            std::string remainder;
            if (TryReadArtSectionHeading(line, heading, remainder)) {
                if (collecting) {
                    break;
                }

                collecting = (heading == sectionName);
                if (collecting && !IsBlankLikeLine(TextLayout::Utf8ToWide(remainder))) {
                    lines.push_back(remainder);
                }
                continue;
            }

            if (collecting) {
                lines.push_back(line);
            }
        }

        if (!lines.empty()) {
            return PreserveArtLayout(lines);
        }
    }

    return fallback;
}

} // namespace

std::vector<std::string> Normalize(const std::vector<std::string>& lines) {
    std::vector<std::wstring> wideLines;
    wideLines.reserve(lines.size());

    int commonLeading = 100000;
    bool foundContent = false;

    for (const std::string& line : lines) {
        std::wstring wideLine = TextLayout::Utf8ToWide(line);
        int leadingCount = 0;
        int contentStart = static_cast<int>(wideLine.size());
        int contentEnd = -1;

        for (int index = 0; index < static_cast<int>(wideLine.size()); ++index) {
            if (!IsBlankLike(wideLine[static_cast<size_t>(index)])) {
                contentStart = index;
                break;
            }
            ++leadingCount;
        }

        for (int index = static_cast<int>(wideLine.size()) - 1; index >= 0; --index) {
            if (!IsBlankLike(wideLine[static_cast<size_t>(index)])) {
                contentEnd = index;
                break;
            }
        }

        if (contentEnd >= contentStart) {
            foundContent = true;
            commonLeading = (std::min)(commonLeading, leadingCount);
        }
        else {
            commonLeading = (std::min)(commonLeading, 0);
        }

        wideLines.push_back(std::move(wideLine));
    }

    if (!foundContent) {
        return {};
    }

    std::vector<std::string> normalized;
    normalized.reserve(wideLines.size());
    for (const std::wstring& wideLine : wideLines) {
        int contentStart = static_cast<int>(wideLine.size());
        int contentEnd = -1;

        for (int index = 0; index < static_cast<int>(wideLine.size()); ++index) {
            if (!IsBlankLike(wideLine[static_cast<size_t>(index)])) {
                contentStart = index;
                break;
            }
        }

        for (int index = static_cast<int>(wideLine.size()) - 1; index >= 0; --index) {
            if (!IsBlankLike(wideLine[static_cast<size_t>(index)])) {
                contentEnd = index;
                break;
            }
        }

        if (contentEnd < contentStart) {
            continue;
        }

        const int trimStart = (std::min)(contentStart, commonLeading);
        normalized.push_back(WideToUtf8(std::wstring(wideLine.begin() + trimStart, wideLine.begin() + contentEnd + 1)));
    }

    return normalized;
}

std::vector<std::string> PreserveArtLayout(const std::vector<std::string>& lines) {
    std::vector<std::wstring> wideLines;
    wideLines.reserve(lines.size());

    int commonLeading = 100000;
    bool foundContent = false;

    for (const std::string& line : lines) {
        std::wstring wideLine = CanonicalizeArtWhitespace(TextLayout::Utf8ToWide(line));
        int leadingCount = 0;
        bool hasVisibleGlyph = false;

        for (wchar_t ch : wideLine) {
            if (!IsBlankLike(ch)) {
                hasVisibleGlyph = true;
                break;
            }
            ++leadingCount;
        }

        if (hasVisibleGlyph) {
            foundContent = true;
            commonLeading = (std::min)(commonLeading, leadingCount);
        }

        wideLines.push_back(std::move(wideLine));
    }

    if (!foundContent) {
        return {};
    }

    if (commonLeading == 100000) {
        commonLeading = 0;
    }

    std::vector<std::string> preserved;
    preserved.reserve(wideLines.size());

    for (const std::wstring& wideLine : wideLines) {
        const int trimStart = (std::min)(commonLeading, static_cast<int>(wideLine.size()));
        preserved.push_back(WideToUtf8(std::wstring(wideLine.begin() + trimStart, wideLine.end())));
    }

    return preserved;
}

const std::vector<std::string>& Get(AsciiArtId id) {
    // Team edit point:
    // replace string rows in the blocks below to swap source art.
    static const std::vector<std::string> kTitleLogo = PreserveArtLayout({
u8"                                                                                                                                                                            ",
u8"                                                                                                                                                                            ",
u8"                                                                                                                                                                            ",
u8"                                                                                                                                                                            ",
u8"                                                                                                                                                                            ",
u8"                                                                                                                                                                            ",
u8"                                                                                                                                                                            ",
u8"                                                                                                                                                                            ",
u8"                                                                                                                                                                            ",
u8"                                                                            1111                                                                                            ",
u8"                                      111111111111111                    1111111                                                                                            ",
u8"                               111111111111111111111111111            111111111        11111               11                                                               ",
u8"                           11111111111111111111111111111111         1111111111       11111111          111111                                                               ",
u8"                       1111111111111              1111111111          11111111       11111111        11111111                                                               ",
u8"                     11111111111        1111        111111111         1111111        1111111          1111111                               111                             ",
u8"                   111111111         111111          11111111         1111111                        1111111    1111 1    1111111111111111111111                            ",
u8"                  1111111         11111111          11111111          1111111             11    11111111111111111111    11111111111111111111111                             ",
u8"                11111111          11111111         11111111           1111111         111111   11111111111111111111    1111111111111111111111                               ",
u8"                111111            11111111        1111111             1111111      111111111   1111111111111           111          1111111                                 ",
u8"               1111111            11111111    111111111              1111111        1111111          111111            11         11111111                                  ",
u8"               1111111     11     1111111  111111111111111111        1111111         111111         1111111                     11111111                                    ",
u8"                1111111    111    1111111 1111   11111111111111      1111111         111111         1111111                   11111111                                      ",
u8"                 111111111111    11111111            11111111111     1111111         111111         1111111                  11111111                                       ",
u8"                   111111111     11111111              111111111    11111111        1111111        1111111                 11111111                                         ",
u8"                                 1111111                11111111    1111111         1111111        1111111               11111111                 11                        ",
u8"                                 1111111                11111111    1111111         111111         1111111             111111111                   11                       ",
u8"                                1111111111111          111111111    1111111        1111111         1111111      11    11111111111111111111       1111                       ",
u8"                                11111111    1         111111111     1111111        1111111         11111111111111    1111111111111111111111111111111                        ",
u8"                               11111111              111111111     111111111   1   111111111111     111111111111    1111111       1111111111111111    11                    ",
u8"                              111111111           11111111111     1111111111111     11111111         11111111       111                 11111      11111                    ",
u8"                             111111111111111111111111111111      1111111111                                                                     1111111                     ",
u8"                         1111111111111111111111111111111                                                                                         111111                     ",
u8"                        11111111      111111111111                                                                        11111                  111111                     ",
u8"                       1111                          111              111111111111          11111111111         111111 1111111111      1111111111111111                     ",
u8"                        11                       11111111          1111111111111111       111111111111111     1111111111111111111    11111111111111111                      ",
u8"                                                1111   11        11111111     111111    11111      111111       11111111     1111  111111      1111111                      ",
u8"                                                111             111111         1111     11111       111111      111111     111    111111        111111                      ",
u8"                                     1111111    1111           111111       111111       1111      1111111      111111           111111         111111                      ",
u8"                                   111    111 11111111        1111111                         11111111111       11111           111111          111111                      ",
u8"                                  111     111   111          1111111                      111111   111111       11111           111111          111111                      ",
u8"                                  111     111  1111          1111111                    11111      111111      111111           1111111         111111                      ",
u8"                                   111  111    111           1111111                   11111       111111      111111            1111111       1111111                      ",
u8"                                    11111      111            1111111             11  111111      1111111       11111             11111111111111111111                      ",
u8"                                              1111            111111111          111   111111111111111111111   111111              111111111111 1111111111                  ",
u8"                                     11       111              1111111111111 111111     111111111   1111111    11111111               111111     11111111                   ",
u8"                                    11      1111                 1111111111111111                                                                                           ",
u8"                                    1111111111                      1111111111                                                                                              ",
u8"                                      11111                                                                                                                                 "
    });
    static const std::vector<std::string> kPlayerBattle = PreserveArtLayout({
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀ ⠀⠀⠀⠤⣄⣠⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀ ⠀⠀⢀⣴⣾⣾⣿⣶⣦⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀ ⠀⠀⠀⠀⠀⣩⣿⣿⣿⣿⣿⣿⣿⣿⣷⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀ ⠀⠀⠀⠀⠀⣸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀ ⠀⠐⣻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⣀⣀⣀⡀⠀⢹⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣯⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠙⠟⠿⣷⣆⣀⣙⣷⣻⣿⣿⣿⣿⣿⣿⡟⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⣀⣤⣶⣶⣶⣶⣬⣉⣻⣿⣿⣿⣿⣿⣿⣿⣿⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⢰⡿⠟⠀⠉⠉⠙⠛⠛⠋⢉⣾⣿⣿⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠸⠁⠀⠀⠀⠀⠀⠀⠀⣀⣾⣿⣿⣿⣿⣿⣿⣿⣿⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⢀⣀⣤⣤⣼⣿⡿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠻⣻⣿⣿⣿⣿⠟⢰⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⣠⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⢀⢰⣾⡿⣿⣿⣿⢋⣴⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠟⣿⣶⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠉⠈⠀⢼⠋⣭⣿⠟⠁⣹⣿⣿⡿⠏⢹⣿⣿⣿⠃⠀⠀⠉⠻⢿⣦⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⣤⣿⠟⠁⠀ ⠀⣿⣿⡿⠀⠀ ⣿⣿⣷⠀⠀⠀⠀ ⠀⠉⠻⢿⣦⣤⡀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠘⠛⠁⠀⠀ ⠀⣰⣿⡟⠀⠀  ⠻⣿⣧⠀⠀⠀  ⠀⠀ ⠀⠈⠙⠿⢷⣦⣤⣀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀  ⠀⠻⣿⡟⠀⠀   ⠀⣿⣿⣦ ⠀ ⠀⠀⠀⠀    ⠀⠀⠉⠛⠛⠃⠀"
    });

    const std::filesystem::path playerAssetPath =
        std::filesystem::path("Assets") / "AsciiArt" / "Player";
    static const std::vector<std::string> kPlayerBattleCombo = LoadPreservedArtSection(
        playerAssetPath / "fighter.txt",
        u8"스탠딩",
        kPlayerBattle);
    static const std::vector<std::string> kPlayerBattleStrength = LoadPreservedArtSection(
        playerAssetPath / "swordsman.txt",
        u8"스탠딩",
        kPlayerBattle);
    static const std::vector<std::string> kPlayerBattleBlock = LoadPreservedArtSection(
        playerAssetPath / "knight.txt",
        u8"스탠딩",
        kPlayerBattle);
    static const std::vector<std::string> kPlayerBattlePoison = LoadPreservedArtSection(
        playerAssetPath / "alchemist.txt",
        u8"스탠딩",
        kPlayerBattle);
    static const std::vector<std::string> kPlayerBattleCycle = LoadPreservedArtSection(
        playerAssetPath / "engineer.txt",
        u8"스탠딩",
        kPlayerBattle);
    static const std::vector<std::string> kPlayerAttackCombo = LoadPreservedArtSection(
        playerAssetPath / "fighter.txt",
        u8"공격",
        kPlayerBattleCombo);
    static const std::vector<std::string> kPlayerAttackStrength = LoadPreservedArtSection(
        playerAssetPath / "swordsman.txt",
        u8"공격",
        kPlayerBattleStrength);
    static const std::vector<std::string> kPlayerAttackBlock = LoadPreservedArtSection(
        playerAssetPath / "knight.txt",
        u8"공격",
        kPlayerBattleBlock);
    static const std::vector<std::string> kPlayerAttackPoison = LoadPreservedArtSection(
        playerAssetPath / "alchemist.txt",
        u8"공격",
        kPlayerBattlePoison);
    static const std::vector<std::string> kPlayerAttackCycle = LoadPreservedArtSection(
        playerAssetPath / "engineer.txt",
        u8"공격",
        kPlayerBattleCycle);
    static const std::vector<std::string> kPlayerDefendCombo = LoadPreservedArtSection(
        playerAssetPath / "fighter.txt",
        u8"방어",
        kPlayerBattleCombo);
    static const std::vector<std::string> kPlayerDefendStrength = LoadPreservedArtSection(
        playerAssetPath / "swordsman.txt",
        u8"방어",
        kPlayerBattleStrength);
    static const std::vector<std::string> kPlayerDefendBlock = LoadPreservedArtSection(
        playerAssetPath / "knight.txt",
        u8"방어",
        kPlayerBattleBlock);
    static const std::vector<std::string> kPlayerDefendPoison = LoadPreservedArtSection(
        playerAssetPath / "alchemist.txt",
        u8"방어",
        kPlayerBattlePoison);
    static const std::vector<std::string> kPlayerDefendCycle = LoadPreservedArtSection(
        playerAssetPath / "engineer.txt",
        u8"방어",
        kPlayerBattleCycle);

    static const std::vector<std::string> kPlayerDeath = PreserveArtLayout({
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀  ⠀⠀⠀⢶⡀⠀⠀⣀⠀⠀⠀⠀   ⠀  ⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢤⣄⡄⠀⠀⡠⣦⣤⣄⡀⠀⠀⠀⣲⣶⣶⣾⣿⣶⣦⣨⣷⠀⠀⠀⠀     ⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠰⣿⣿⣦⡶⠾⡶⢾⣿⣿⣆⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣂⠀⠀⠀     ⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢶⣾⣿⣿⣿⣷⣶⣤⣦⣽⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣆⠀⠀⠀    ",
        u8"⠀⠀⠀⠀⠀⠀⢀⣀⣀⢀⣼⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠛⠀⠀    ⠀",
        u8"⢀⣴⣷⣶⣶⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⣀⠀  ⠀",
        u8"⠉⠛⢿⣻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⣶⠮⣦⡀  ",
        u8"⠀⠀⠿⠿⠿⠿⠿⠿⠿⠛⢿⠫⣟⣛⣯⣿⣯⣧⣤⠀⠈⠉⠹⠻⠿⠿⣿⣿⠏⠀⠉⠉⠉⠉⠉⠀⠀⠁   ",
        u8"⠀⠀⠀⠀⠀⣦⣴⣶⣾⣿⠿⠿⠿⠛⠛⠋⠉⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀ ⠀"
    });
    static const std::vector<std::string> kPlayerDeathCombo = LoadPreservedArtSection(
        playerAssetPath / "fighter.txt",
        u8"죽음",
        kPlayerDeath);
    static const std::vector<std::string> kPlayerDeathStrength = LoadPreservedArtSection(
        playerAssetPath / "swordsman.txt",
        u8"죽음",
        kPlayerDeath);
    static const std::vector<std::string> kPlayerDeathBlock = LoadPreservedArtSection(
        playerAssetPath / "knight.txt",
        u8"죽음",
        kPlayerDeath);
    static const std::vector<std::string> kPlayerDeathPoison = LoadPreservedArtSection(
        playerAssetPath / "alchemist.txt",
        u8"죽음",
        kPlayerDeath);
    static const std::vector<std::string> kPlayerDeathCycle = LoadPreservedArtSection(
        playerAssetPath / "engineer.txt",
        u8"죽음",
        kPlayerDeath);

    static const std::vector<std::string> kPlayerCardPack = kPlayerBattle;

    static const std::vector<std::string> kEnemyNormal = PreserveArtLayout({
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⣶⣶⣶⣶⣶⣶⣶⣦⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣴⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣦⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣴⣿⣿⡋⠀⠀⢸⣿⣿⣿⣿⣿⣿⣿⡿⢿⣿⣿⣷⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⣠⣾⣿⣿⣿⣿⣶⣶⣿⣿⣿⣿⣿⣿⣿⣿⣏⠀⠀⠈⠻⣿⣿⣧⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⢠⣴⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⡤⠀⠤⣿⣿⣿⣧⣄⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⣰⣿⣿⣫⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⡀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⢰⣿⣿⣿⣿⣿⣿⣿⣿⣿⠿⠻⠿⢿⣿⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣄⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠘⣿⣿⣿⠻⠛⠻⢶⣀⢠⣴⣾⣿⣶⣶⣶⣶⣿⣿⣟⣿⢿⣿⣿⣿⣿⣿⣿⣿⣿⣏⣻⣿⣿⣆⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠘⢻⣿⣧⣤⣄⠀⢷⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣵⡿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠙⠻⠿⣷⣾⣿⣿⣿⣿⣿⣿⣿⣿⡿⢿⣿⣿⣿⣿⣿⣷⣖⠎⢛⠻⠿⣿⣿⣿⣿⡿⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⣿⡛⢻⣿⣿⣿⣿⡿⠛⠀⢰⣿⣿⣿⣿⣿⣿⣷⣥⠖⣷⣾⣿⡿⠟⠛⠁⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⣀⣤⣼⣿⣿⣷⣤⣿⣿⣿⣿⣿⣤⣤⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⡁⠁⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⢀⣰⣾⣿⣿⣿⡿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡎⢻⣿⣿⣿⣿⣦⡀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠠⣾⣿⣿⣿⣿⣯⠗⣹⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⣰⣿⣿⣿⣿⣿⠇⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⢠⢿⠿⣿⠍⠹⠟⢤⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣾⣿⣿⣿⣿⣿⣇⠻⢟⣿⣿⣿⡿⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠙⠀⠀⠰⣿⣿⣿⣿⠟⠻⣿⣿⣿⣿⣿⡟⠻⣿⣿⣿⣿⣿⣄⠁⠋⠉⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⣤⣶⣿⣿⣿⣽⣿⡁⠐⠛⠘⠛⠀⠀⢀⣸⣿⣿⣿⣿⣯⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠛⡛⢛⡿⡟⠛⠛⠛⠋⠀⠀⠀⠀⠀⠔⠿⡻⢿⣿⣿⠿⠿⠿⠆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
    });

    const std::filesystem::path normalMonsterAssetPath =
        std::filesystem::path("Assets") / "AsciiArt" / "Normal" / "monster-assets.txt";
    static const std::vector<std::string> kEnemyNormalGoblin = LoadPreservedArtSection(
        normalMonsterAssetPath,
        u8"고블린",
        kEnemyNormal);
    static const std::vector<std::string> kEnemyNormalSkeleton = LoadPreservedArtSection(
        normalMonsterAssetPath,
        u8"스켈레톤",
        kEnemyNormal);
    static const std::vector<std::string> kEnemyNormalGolem = LoadPreservedArtSection(
        normalMonsterAssetPath,
        u8"골렘",
        kEnemyNormal);
    static const std::vector<std::string> kEnemyNormalBat = LoadPreservedArtSection(
        normalMonsterAssetPath,
        u8"박쥐",
        kEnemyNormal);
    static const std::vector<std::string> kEnemyNormalMushroom = LoadPreservedArtSection(
        normalMonsterAssetPath,
        u8"머쉬룸",
        kEnemyNormal);
    static const std::vector<std::string> kEnemyNormalSlime = LoadPreservedArtSection(
        normalMonsterAssetPath,
        u8"슬라임",
        kEnemyNormal);

    static const std::vector<std::string> kEnemyElite = PreserveArtLayout({
        u8"⠀⠀⠀⠀⣠⣶⣶⣦",
        u8"⠀⠀⣠⣾⣿⣿⣿⣿⣷⣄",
        u8"⠀⢰⣿⣿⣿⣿⣿⣿⣿⣿⡆",
        u8"⠀⠈⢿⣿⣿⣿⣿⣿⣿⡿⠁",
        u8"⠀⠀⠀⣿⣿⣿⣿⣿⣿",
        u8"⠀⠀⣰⣿⣿⣿⣿⣿⣿⣆",
        u8"⠀⣴⣿⣿⡿⠋⠀⠙⢿⣿⣦",
        u8"⠀⠿⠛⠁⠀⠀⠀⠀⠀⠈⠛⠿"
    });

    static const std::vector<std::string> kEnemyBoss = PreserveArtLayout({
        u8"⠀⠀⠹⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠃⠀⣠⣿⣿⣿⣿⣦⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠿⠛⠉⠁",
        u8"⠀⠀⠀⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣦⣀⣾⣿⣿⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠙⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠈⢻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠙⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣴⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠈⠛⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡟⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠛⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣧⠀⠀⠀⠀⠀⠀⠀⠀⢠⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣆⠀⠀⠀⠀⠀⠀⣰⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⣿⣿⣿⣿⣿⣿⣿⣿⣿⡄⠀⠀⠀⢀⣼⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠁⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢹⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠁⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣤⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡏⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⣠⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣇⠀⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⢀⣴⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡄⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⢀⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⢀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⣸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⢀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠁⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀⠀⠀",
        u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠙⠛⠿⠿⠛⠋"
    });

    static const std::vector<std::string> kEnemyBossCentaurus = LoadPreservedArtFile(
        std::filesystem::path("Assets") / "AsciiArt" / "Boss" / "centaurus.txt",
        kEnemyBoss);
    static const std::vector<std::string> kEnemyBossPuppet = LoadPreservedArtFile(
        std::filesystem::path("Assets") / "AsciiArt" / "Boss" / "puppet.txt",
        kEnemyBoss);
    static const std::vector<std::string> kEnemyBossHydra = LoadPreservedArtFile(
        std::filesystem::path("Assets") / "AsciiArt" / "Boss" / "hydra.txt",
        kEnemyBoss);

    // 카드팩 씬 니오우는 일부만 화면에 걸치게 쓰는 전제.
    // 오른쪽 영역만 보이도록 main.cpp에서 클립 렌더링.
    static const std::vector<std::string> kNeow = PreserveArtLayout({
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣀⢀⣀⣀⣀⣤⣤⣤⣤⣤⣴⣤⣶⣦⣴⣴⣦⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣶⣶⣶⣶⣤⣤⣤⣄⣀⣀⣀⣀⣀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀  ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣴⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀  ⠀⠀⠀⠀⠀⣀⣀⣤⣴⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀  ⠀⠀⠀⠀⣀⣤⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀  ⠀⠀⠀⠀⢀⣤⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀  ⠀⠀⠀⣠⣶⣿⣿⡿⠯⠙⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀  ⠀⠀⠀⣠⣾⣿⡏⠉⠀⠀⠀  ⣿⣿⣿⣿⣿⣿⡿⠛⠉⠀⠀⣹⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠀⠀⠀  ⠀⠀⢀⣾⣿⠛⠁⠀⠀⠀⠀⠀⢸⣿⣿⣿⣿⣿⣿⡁⠀⣀⣠⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠀⠀  ⠀⠀⣠⣿⡿⠁  ⠀⠀⠀⠀⠀⣠⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀  ⠀⠀⠀⣴⣿⣿⡇  ⠀⠀⠀⠀  ⣴⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀ ⠀⢀⣾⣿⣿⣿⣿⣷⣦⣤⣤⣤⣾⣿⣿⡿⠛⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡟⣿⡿⠿⣛⣵⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀ ⠀⢠⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡏⠀⢀⣰⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣳⣭⣶⣿⣿⣿⠿⢟⣽⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀ ⠀⢠⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢽⠟⣫⣭⣷⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠟⣠⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⢠⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⣸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣇⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠀⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠀⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⣽⣿⣿⣿⣿⣿⣟⠉⠉⠉⠙⣻⣿⣿⣿⣿⣿⣿⡿⠟⠉⠙⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠀⠈⢿⣿⣿⣿⡿⠿⢿⣿⠿⠿⣿⣿⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣶⣶⣶⣿⣿⣿⣿⣿⣿⣿⣿⣦⣤⣴⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠀⠀⠈⠓⠀⠀⠁⠀⠀⠁⠀⠀⠀⠉⠉⠀⠀⠽⠛⠛⢿⠿⠛⠿⡿⠿⠿⢿⠿⣿⠿⠿⡿⡿⠿⢿⡟⠻⠿⣿⣿⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠀⠀⢀⣤⠄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠃⠀⠀⠈⠉⠙⠛⣿⢟⠟⢻⠿⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠀⠀⣼⣿⣷⣾⣦⣷⣄⣤⣠⣀⡀⢀⠀⠀⠀⠀⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠁⣀⣄⠀⡠⣀⣈⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠀⠀⠙⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣿⣷⣿⣷⣶⣶⣦⣴⣤⣶⣤⣄⣴⣄⣄⣤⣶⣤⣤⣄⣴⣶⣤⣤⣤⣤⣤⣦⣦⣴⣾⣷⣴⣾⣿⣷⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠘⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠙⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠙⠛⠛⠿⠿⠿⠿⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠿⠿⠿⠿⠿⠿⠋⠉⠉⠀"
    });

    static const std::vector<std::string> kNeowAngel = LoadPreservedArtFile(
        std::filesystem::path("Assets") / "AsciiArt" / "Neow" / "angel.txt",
        kNeow);

    static const std::vector<std::string> kMerchant = PreserveArtLayout({
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣴⣶⣶⣶⣤⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⣿⣿⣿⣿⣿⣷⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢿⣿⣿⣿⣿⣿⣿⣾⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢘⣿⣿⣿⣿⡟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⢀⣤⣤⣶⣶⣶⣾⣷⣶⣶⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢰⣿⣿⣿⣿⣿⣧⣦⣠⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⢠⣿⣿⣿⣿⣿⣿⣿⣿⢿⣿⠛⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣻⣿⣿⣿⣿⣿⣿⣿⣿⠄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⣿⣍⠉⠉⠉⢀⣀⣈⣤⣼⣿⠀⠀⠀⠀⠀⠀⠀⠀⡀⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣆⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣶⣶⣄⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠘⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⣤⣤⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣦⡄⠀⠀⠀⠀⠀⠀⣤⣾⣿⣿⣿⣏⠀⠂⠀⡀⠀⠀",
u8"⠀⠀⠀⠙⣿⣿⣿⣿⣿⣿⣿⠿⠿⠛⠀⠀⠀⢀⠀⠀⠀⠀⣼⣿⣿⣿⣿⣾⡿⢿⣿⠟⣫⣷⣿⣿⣿⡄⠀⠀⠀⠀⠘⢿⣿⣿⣿⣿⡿⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠈⠛⠉⠉⠉⠀⠀⠀⠀⠀⠀⣴⣾⣷⣶⣶⣬⢤⣿⣿⣿⣿⣿⣿⣗⠀⠀⢰⣿⣿⣿⣿⡷⡇⠀⠀⠠⠀⠀⠀⠉⠙⠛⢉⠁⠀⠂⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣶⣿⣿⣿⣿⣿⠉⠀⠈⢿⣿⣬⣿⣿⣿⣿⣷⣤⢸⠟⠛⠛⠛⠋⠁⠀⠀⠀⢤⠄⠁⠄⠐⢀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣤⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣤⣄⣉⣋⡁⠀⠀⠘⣿⠛⠘⠀⠀⣀⣠⣤⣶⣿⣽⣶⣽⣳⡄⣐⣆⠀⠤⠄⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣶⣶⣶⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⢿⡟⢰⢤⣤⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣗⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣿⣿⣿⣶⠋⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠘⠯⠭⠉⢻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠇⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠉⠉⠉⠛⠻⠿⠿⠿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡟⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠉⠉⠉⠉⠉⠛⠻⠿⠿⠥⠄⢿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
    });

    static const std::vector<std::string> kTreasureChestClosed = PreserveArtLayout({
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⡠⠴⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⡒⣢⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⡴⠭⠬⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⠶⣒⠝⠉⠘⢧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⡟⠀⠀⠀⠈⢧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢰⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡇⠀⠀⠀⠀⢈⢧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡟⠀⠀⠀⠀⠀⠀⠀⠀⡏⠢⢄⡠⠔⢹⠀⠀⠀⠀⠀⠀⠀⠀⠀⣾⠀⠀⣀⣤⡲⠕⣼⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⢣⣤⣤⣤⣤⣤⣤⣤⣤⡃⠀⠀⠀⠀⠘⣤⠤⠤⠤⡤⠤⠤⠤⢤⢋⠮⠛⠉⠀⠀⢰⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⣇⠀⠀⠀⠀⠀⠀⠀⠀⠈⢆⡔⠒⠊⠉⠀⠀⠀⠀⠀⠀⠀⠀⠈⡾⡀⠀⠀⠀⠀⢸⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⣇⠀⠀⠀⠀⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢷⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⡄⠀⠀⢰⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⡧⣀⣤⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣤⣀⣀⣀⣀⣀⣀⣀⣠⣸⠑⡴⠞⠳⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠑⠊⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠓⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
    });

    static const std::vector<std::string> kTreasureChestOpen = PreserveArtLayout({
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⡤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⣤⠤⣤⣤⣤⣤⣄⣠⡤⢄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⣦⠛⡦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢹⡄⠨⣣⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠐⣻⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢷⠀⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠱⠇⠀⠀⠀⡀⠀⠀⠀⠀⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⡍⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡠⠖⠋⠀⠀⠀⠀⡏⠒⢄⡠⠔⢹⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⠴⠚⣼⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⢒⣭⣤⣤⣤⣤⣤⣤⣤⡃⠀⠀⠀⠀⠘⡤⠤⠤⠤⠤⠤⠤⠤⢤⢤⠶⠛⠁⠀⠀⢸⡜⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⣇⠀⠀⠀⠀⠀⠀⠀⠀⠈⠢⡔⠒⠊⠉⠀⠀⠀⠀⠀⠀⠀⠀⠈⡾⡀⠀⠀⠀⠀⢸⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⣇⠀⠀⠀⠀⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⣼⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⡄⠀⠀⢸⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠱⡣⢄⣠⣄⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣠⣸⠑⡴⠞⠛⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀",
u8"⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠑⠊⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠓⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
    });

    static const std::vector<std::string> kCampfire = PreserveArtLayout({
u8"           (  )    ( )",
u8"          (    )  (   )",
u8"         (  **  )(  ** )",
u8"            \\  ||  //",
u8"             \\ || //",
u8"          ____\\||//____",
u8"         /_____/\\\\_____\\",
u8"              /__\\\\"
    });

    switch (id) {
    case AsciiArtId::TitleLogo: return kTitleLogo;
    case AsciiArtId::PlayerBattle: return kPlayerBattle;
    case AsciiArtId::PlayerBattleCombo: return kPlayerBattleCombo;
    case AsciiArtId::PlayerBattleStrength: return kPlayerBattleStrength;
    case AsciiArtId::PlayerBattleBlock: return kPlayerBattleBlock;
    case AsciiArtId::PlayerBattlePoison: return kPlayerBattlePoison;
    case AsciiArtId::PlayerBattleCycle: return kPlayerBattleCycle;
    case AsciiArtId::PlayerAttackCombo: return kPlayerAttackCombo;
    case AsciiArtId::PlayerAttackStrength: return kPlayerAttackStrength;
    case AsciiArtId::PlayerAttackBlock: return kPlayerAttackBlock;
    case AsciiArtId::PlayerAttackPoison: return kPlayerAttackPoison;
    case AsciiArtId::PlayerAttackCycle: return kPlayerAttackCycle;
    case AsciiArtId::PlayerDefendCombo: return kPlayerDefendCombo;
    case AsciiArtId::PlayerDefendStrength: return kPlayerDefendStrength;
    case AsciiArtId::PlayerDefendBlock: return kPlayerDefendBlock;
    case AsciiArtId::PlayerDefendPoison: return kPlayerDefendPoison;
    case AsciiArtId::PlayerDefendCycle: return kPlayerDefendCycle;
    case AsciiArtId::PlayerDeath: return kPlayerDeath;
    case AsciiArtId::PlayerDeathCombo: return kPlayerDeathCombo;
    case AsciiArtId::PlayerDeathStrength: return kPlayerDeathStrength;
    case AsciiArtId::PlayerDeathBlock: return kPlayerDeathBlock;
    case AsciiArtId::PlayerDeathPoison: return kPlayerDeathPoison;
    case AsciiArtId::PlayerDeathCycle: return kPlayerDeathCycle;
    case AsciiArtId::PlayerCardPack: return kPlayerCardPack;
    case AsciiArtId::EnemyNormal: return kEnemyNormalGoblin;
    case AsciiArtId::EnemyNormalGoblin: return kEnemyNormalGoblin;
    case AsciiArtId::EnemyNormalSkeleton: return kEnemyNormalSkeleton;
    case AsciiArtId::EnemyNormalGolem: return kEnemyNormalGolem;
    case AsciiArtId::EnemyNormalBat: return kEnemyNormalBat;
    case AsciiArtId::EnemyNormalMushroom: return kEnemyNormalMushroom;
    case AsciiArtId::EnemyNormalSlime: return kEnemyNormalSlime;
    case AsciiArtId::EnemyElite: return kEnemyElite;
    case AsciiArtId::EnemyBoss: return kEnemyBossCentaurus;
    case AsciiArtId::EnemyBossCentaurus: return kEnemyBossCentaurus;
    case AsciiArtId::EnemyBossPuppet: return kEnemyBossPuppet;
    case AsciiArtId::EnemyBossHydra: return kEnemyBossHydra;
    case AsciiArtId::Neow: return kNeowAngel;
    case AsciiArtId::Merchant: return kMerchant;
    case AsciiArtId::TreasureChestClosed: return kTreasureChestClosed;
    case AsciiArtId::TreasureChestOpen: return kTreasureChestOpen;
    case AsciiArtId::Campfire: return kCampfire;
    default: return kPlayerBattle;
    }
}

const std::vector<std::string>& GetPlayerBattle(CardArchetype archetype) {
    switch (archetype) {
    case CardArchetype::Combo: return Get(AsciiArtId::PlayerBattleCombo);
    case CardArchetype::Strength: return Get(AsciiArtId::PlayerBattleStrength);
    case CardArchetype::Block: return Get(AsciiArtId::PlayerBattleBlock);
    case CardArchetype::Poison: return Get(AsciiArtId::PlayerBattlePoison);
    case CardArchetype::Cycle: return Get(AsciiArtId::PlayerBattleCycle);
    default: return Get(AsciiArtId::PlayerBattle);
    }
}

const std::vector<std::string>& GetPlayerAttack(CardArchetype archetype) {
    switch (archetype) {
    case CardArchetype::Combo: return Get(AsciiArtId::PlayerAttackCombo);
    case CardArchetype::Strength: return Get(AsciiArtId::PlayerAttackStrength);
    case CardArchetype::Block: return Get(AsciiArtId::PlayerAttackBlock);
    case CardArchetype::Poison: return Get(AsciiArtId::PlayerAttackPoison);
    case CardArchetype::Cycle: return Get(AsciiArtId::PlayerAttackCycle);
    default: return GetPlayerBattle(archetype);
    }
}

const std::vector<std::string>& GetPlayerDefend(CardArchetype archetype) {
    switch (archetype) {
    case CardArchetype::Combo: return Get(AsciiArtId::PlayerDefendCombo);
    case CardArchetype::Strength: return Get(AsciiArtId::PlayerDefendStrength);
    case CardArchetype::Block: return Get(AsciiArtId::PlayerDefendBlock);
    case CardArchetype::Poison: return Get(AsciiArtId::PlayerDefendPoison);
    case CardArchetype::Cycle: return Get(AsciiArtId::PlayerDefendCycle);
    default: return GetPlayerBattle(archetype);
    }
}

const std::vector<std::string>& GetPlayerDeath(CardArchetype archetype) {
    switch (archetype) {
    case CardArchetype::Combo: return Get(AsciiArtId::PlayerDeathCombo);
    case CardArchetype::Strength: return Get(AsciiArtId::PlayerDeathStrength);
    case CardArchetype::Block: return Get(AsciiArtId::PlayerDeathBlock);
    case CardArchetype::Poison: return Get(AsciiArtId::PlayerDeathPoison);
    case CardArchetype::Cycle: return Get(AsciiArtId::PlayerDeathCycle);
    default: return Get(AsciiArtId::PlayerDeath);
    }
}

const std::vector<std::string>& GetRandomEnemyBoss() {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> distribution(0, 2);

    switch (distribution(rng)) {
    case 0: return Get(AsciiArtId::EnemyBossCentaurus);
    case 1: return Get(AsciiArtId::EnemyBossPuppet);
    default: return Get(AsciiArtId::EnemyBossHydra);
    }
}

const std::vector<std::string>& GetRandomEnemyNormal() {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> distribution(0, 5);

    switch (distribution(rng)) {
    case 0: return Get(AsciiArtId::EnemyNormalGoblin);
    case 1: return Get(AsciiArtId::EnemyNormalSkeleton);
    case 2: return Get(AsciiArtId::EnemyNormalGolem);
    case 3: return Get(AsciiArtId::EnemyNormalBat);
    case 4: return Get(AsciiArtId::EnemyNormalMushroom);
    default: return Get(AsciiArtId::EnemyNormalSlime);
    }
}

} // namespace AsciiArtLibrary
