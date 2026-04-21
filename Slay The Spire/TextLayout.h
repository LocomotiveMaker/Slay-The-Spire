// -----------------------------------------------------------------------------
// @file       TextLayout.h
// @brief      UTF-8 text measurement and wrapping helpers for console UI.
// -----------------------------------------------------------------------------
#pragma once
#include <string>
#include <vector>

namespace TextLayout {

enum class HorizontalAlign {
    Left,
    Center,
    Right
};

struct WrappedText {
    std::vector<std::wstring> lines;
    int maxLineWidth = 0;
};

std::wstring Utf8ToWide(const std::string& utf8Text);

int GetCellWidth(wchar_t ch);
int MeasureDisplayWidth(const std::wstring& text);
int MeasureDisplayWidthUtf8(const std::string& utf8Text);

std::wstring TrimToWidth(const std::wstring& text, int maxWidth);
std::wstring AlignToWidth(const std::wstring& text, int targetWidth, HorizontalAlign align, wchar_t fill = L' ');

WrappedText WrapText(const std::wstring& text, int maxWidth);
WrappedText WrapUtf8(const std::string& utf8Text, int maxWidth);

int ComputeAlignedX(int left, int areaWidth, int contentWidth, HorizontalAlign align);
int ComputeAlignedX(int left, int areaWidth, const std::wstring& text, HorizontalAlign align);
int ComputeAlignedXUtf8(int left, int areaWidth, const std::string& utf8Text, HorizontalAlign align);
int ComputeAlignedY(int top, int areaHeight, int contentHeight);

int ComputeBoxWidth(const std::vector<std::string>& utf8Lines, int horizontalPadding = 0);
int ComputeBoxHeight(int lineCount, int verticalPadding = 0);

} // namespace TextLayout
