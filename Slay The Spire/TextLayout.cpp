// -----------------------------------------------------------------------------
// @file       TextLayout.cpp
// -----------------------------------------------------------------------------
#include "TextLayout.h"
#include <algorithm>
#include <cwctype>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace TextLayout {
namespace {

wchar_t SanitizeGlyph(wchar_t ch) {
    if (ch == L'\r' || ch == L'\n') {
        return ch;
    }

    if (ch == L'\t') {
        return L' ';
    }

    if (ch < 0x20 || !iswprint(ch)) {
        return L'?';
    }

    return ch;
}

bool IsFullWidthGlyph(wchar_t ch) {
    return (ch >= 0x1100 &&
        (ch <= 0x115F ||
            ch == 0x2329 || ch == 0x232A ||
            (ch >= 0x2E80 && ch <= 0xA4CF && ch != 0x303F) ||
            (ch >= 0xAC00 && ch <= 0xD7A3) ||
            (ch >= 0xF900 && ch <= 0xFAFF) ||
            (ch >= 0xFE10 && ch <= 0xFE19) ||
            (ch >= 0xFE30 && ch <= 0xFE6F) ||
            (ch >= 0xFF00 && ch <= 0xFF60) ||
            (ch >= 0xFFE0 && ch <= 0xFFE6)));
}

void PushWrappedLine(WrappedText& wrapped, std::wstring& currentLine, int& currentWidth) {
    wrapped.lines.push_back(currentLine);
    wrapped.maxLineWidth = (std::max)(wrapped.maxLineWidth, currentWidth);
    currentLine.clear();
    currentWidth = 0;
}

} // namespace

std::wstring Utf8ToWide(const std::string& utf8Text) {
    if (utf8Text.empty()) {
        return {};
    }

    int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8Text.data(), static_cast<int>(utf8Text.size()), nullptr, 0);
    if (wideLength <= 0) {
        wideLength = MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), static_cast<int>(utf8Text.size()), nullptr, 0);
    }

    if (wideLength <= 0) {
        return L"?";
    }

    std::wstring wideText(static_cast<size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), static_cast<int>(utf8Text.size()), wideText.data(), wideLength);

    for (wchar_t& ch : wideText) {
        ch = SanitizeGlyph(ch);
    }

    return wideText;
}

int GetCellWidth(wchar_t ch) {
    ch = SanitizeGlyph(ch);

    if (ch == L'\r' || ch == L'\n') {
        return 0;
    }

    return IsFullWidthGlyph(ch) ? 2 : 1;
}

int MeasureDisplayWidth(const std::wstring& text) {
    int width = 0;
    for (wchar_t ch : text) {
        width += GetCellWidth(ch);
    }
    return width;
}

int MeasureDisplayWidthUtf8(const std::string& utf8Text) {
    return MeasureDisplayWidth(Utf8ToWide(utf8Text));
}

std::wstring TrimToWidth(const std::wstring& text, int maxWidth) {
    if (maxWidth <= 0) {
        return {};
    }

    std::wstring trimmed;
    int currentWidth = 0;

    for (wchar_t ch : text) {
        if (ch == L'\r' || ch == L'\n') {
            break;
        }

        const int glyphWidth = GetCellWidth(ch);
        if (currentWidth + glyphWidth > maxWidth) {
            break;
        }

        trimmed.push_back(SanitizeGlyph(ch));
        currentWidth += glyphWidth;
    }

    return trimmed;
}

std::wstring AlignToWidth(const std::wstring& text, int targetWidth, HorizontalAlign align, wchar_t fill) {
    const std::wstring clipped = TrimToWidth(text, targetWidth);
    const int currentWidth = MeasureDisplayWidth(clipped);
    const int remaining = (std::max)(0, targetWidth - currentWidth);

    int leftPadding = 0;
    int rightPadding = 0;

    switch (align) {
    case HorizontalAlign::Center:
        leftPadding = remaining / 2;
        rightPadding = remaining - leftPadding;
        break;
    case HorizontalAlign::Right:
        leftPadding = remaining;
        break;
    case HorizontalAlign::Left:
    default:
        rightPadding = remaining;
        break;
    }

    return std::wstring(static_cast<size_t>(leftPadding), fill) +
        clipped +
        std::wstring(static_cast<size_t>(rightPadding), fill);
}

WrappedText WrapText(const std::wstring& text, int maxWidth) {
    WrappedText wrapped;
    if (maxWidth <= 0) {
        wrapped.lines.push_back({});
        return wrapped;
    }

    std::wstring currentLine;
    int currentWidth = 0;

    for (wchar_t rawCh : text) {
        const wchar_t ch = SanitizeGlyph(rawCh);

        if (ch == L'\r') {
            continue;
        }

        if (ch == L'\n') {
            PushWrappedLine(wrapped, currentLine, currentWidth);
            continue;
        }

        const int glyphWidth = GetCellWidth(ch);
        if (currentWidth > 0 && currentWidth + glyphWidth > maxWidth) {
            PushWrappedLine(wrapped, currentLine, currentWidth);
        }

        currentLine.push_back(ch);
        currentWidth += glyphWidth;
    }

    if (!currentLine.empty() || wrapped.lines.empty()) {
        PushWrappedLine(wrapped, currentLine, currentWidth);
    }

    return wrapped;
}

WrappedText WrapUtf8(const std::string& utf8Text, int maxWidth) {
    return WrapText(Utf8ToWide(utf8Text), maxWidth);
}

int ComputeAlignedX(int left, int areaWidth, int contentWidth, HorizontalAlign align) {
    const int remaining = (std::max)(0, areaWidth - contentWidth);

    switch (align) {
    case HorizontalAlign::Center:
        return left + (remaining / 2);
    case HorizontalAlign::Right:
        return left + remaining;
    case HorizontalAlign::Left:
    default:
        return left;
    }
}

int ComputeAlignedX(int left, int areaWidth, const std::wstring& text, HorizontalAlign align) {
    return ComputeAlignedX(left, areaWidth, MeasureDisplayWidth(text), align);
}

int ComputeAlignedXUtf8(int left, int areaWidth, const std::string& utf8Text, HorizontalAlign align) {
    return ComputeAlignedX(left, areaWidth, MeasureDisplayWidthUtf8(utf8Text), align);
}

int ComputeAlignedY(int top, int areaHeight, int contentHeight) {
    return top + (std::max)(0, (areaHeight - contentHeight) / 2);
}

int ComputeBoxWidth(const std::vector<std::string>& utf8Lines, int horizontalPadding) {
    int maxLineWidth = 0;
    for (const std::string& line : utf8Lines) {
        maxLineWidth = (std::max)(maxLineWidth, MeasureDisplayWidthUtf8(line));
    }

    return maxLineWidth + (std::max)(0, horizontalPadding) * 2;
}

int ComputeBoxHeight(int lineCount, int verticalPadding) {
    return (std::max)(0, lineCount) + (std::max)(0, verticalPadding) * 2;
}

} // namespace TextLayout
