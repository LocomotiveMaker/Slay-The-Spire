// -----------------------------------------------------------------------------
// @file       MapRenderer.cpp
// @brief      지도 노드/경로 렌더링 및 스크롤 처리 구현부
// -----------------------------------------------------------------------------
#include "MapRenderer.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

constexpr float kPi = 3.1415926535f;

struct RectI {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct FloatPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct IntPoint {
    int x = 0;
    int y = 0;
};

float Clamp01(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

bool IsInsideRect(int x, int y, const RectI& rect) {
    return x >= rect.x &&
        x < rect.x + rect.width &&
        y >= rect.y &&
        y < rect.y + rect.height;
}

bool IsRectVisible(int x, int y, int width, int height, const RectI& rect) {
    return !(x + width <= rect.x ||
        x >= rect.x + rect.width ||
        y + height <= rect.y ||
        y >= rect.y + rect.height);
}

void DrawCharClipped(ScreenManager& screen, int x, int y, wchar_t ch, WORD color, const RectI& clipRect) {
    if (!IsInsideRect(x, y, clipRect)) {
        return;
    }

    if (x < 0 || x >= screen.GetWidth() || y < 0 || y >= screen.GetHeight()) {
        return;
    }

    screen.DrawChar(x, y, ch, color);
}

void DrawWideTextClipped(ScreenManager& screen, int x, int y, const std::wstring& text, WORD color, const RectI& clipRect) {
    if (y < clipRect.y || y >= clipRect.y + clipRect.height || y < 0 || y >= screen.GetHeight()) {
        return;
    }

    int cursorX = x;
    for (wchar_t ch : text) {
        const int cellWidth = TextLayout::GetCellWidth(ch);
        if (cellWidth <= 0) {
            continue;
        }

        const bool fitsViewport = cursorX >= clipRect.x && (cursorX + cellWidth) <= (clipRect.x + clipRect.width);
        const bool fitsScreen = cursorX >= 0 && (cursorX + cellWidth) <= screen.GetWidth();
        if (fitsViewport && fitsScreen) {
            screen.DrawChar(cursorX, y, ch, color);
        }

        cursorX += cellWidth;
    }
}

float HashToUnitRange(int hashValue) {
    const int positive = std::abs(hashValue % 2001);
    return (static_cast<float>(positive) / 1000.0f) - 1.0f;
}

FloatPoint QuadraticBezier(const FloatPoint& p0, const FloatPoint& p1, const FloatPoint& p2, float t) {
    const float omt = 1.0f - t;
    return {
        (omt * omt * p0.x) + (2.0f * omt * t * p1.x) + (t * t * p2.x),
        (omt * omt * p0.y) + (2.0f * omt * t * p1.y) + (t * t * p2.y)
    };
}

void AppendPointIfNeeded(std::vector<IntPoint>& outPoints, int x, int y) {
    if (!outPoints.empty() && outPoints.back().x == x && outPoints.back().y == y) {
        return;
    }

    outPoints.push_back({ x, y });
}

void RasterizeSegment(const FloatPoint& from, const FloatPoint& to, std::vector<IntPoint>& outPoints) {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const int steps = (std::max)(1, static_cast<int>(std::ceil((std::max)(std::fabs(dx), std::fabs(dy)))));

    for (int step = 0; step <= steps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(steps);
        const int x = static_cast<int>(std::round(from.x + (dx * t)));
        const int y = static_cast<int>(std::round(from.y + (dy * t)));
        AppendPointIfNeeded(outPoints, x, y);
    }
}

wchar_t SelectPathGlyph(const IntPoint* previous, const IntPoint& current, const IntPoint& next) {
    const int forwardDx = next.x - current.x;
    if (forwardDx == 0) {
        return L'|';
    }

    const int previousDx = (previous != nullptr) ? (current.x - previous->x) : forwardDx;
    const bool turning = (previous != nullptr) && ((previousDx > 0 && forwardDx < 0) || (previousDx < 0 && forwardDx > 0));

    if (forwardDx > 0) {
        return turning ? L'/' : L'╱';
    }

    return turning ? L'\\' : L'╲';
}

std::vector<IntPoint> BuildPathPoints(int startX, int startY, int endX, int endY, int hashValue) {
    std::vector<IntPoint> points;
    const float dx = static_cast<float>(endX - startX);
    const float dy = static_cast<float>(endY - startY);
    const float phase = (HashToUnitRange(hashValue * 31) + 1.0f) * 0.5f;
    const float sway = HashToUnitRange(hashValue * 17);
    const float curveBias = std::sin((phase + 0.15f) * kPi) * (std::min)(4.0f, std::fabs(dx) * 0.18f + 1.0f);
    const float waveAmplitude = std::sin((phase + 0.4f) * kPi) * (std::min)(1.8f, std::fabs(dx) * 0.05f + 0.4f);

    const FloatPoint p0 = { static_cast<float>(startX), static_cast<float>(startY) };
    const FloatPoint p2 = { static_cast<float>(endX), static_cast<float>(endY) };
    const FloatPoint p1 = {
        ((p0.x + p2.x) * 0.5f) + curveBias + (sway * 1.5f),
        ((p0.y + p2.y) * 0.5f) + (std::sin((phase + 0.6f) * kPi) * 1.5f)
    };

    const int sampleCount = (std::max)(
        18,
        static_cast<int>(std::ceil((std::max)(std::fabs(dx), std::fabs(dy))) * 1.7f));

    FloatPoint previousSample = p0;
    AppendPointIfNeeded(points, static_cast<int>(std::round(p0.x)), static_cast<int>(std::round(p0.y)));

    for (int sampleIndex = 1; sampleIndex <= sampleCount; ++sampleIndex) {
        const float t = static_cast<float>(sampleIndex) / static_cast<float>(sampleCount);
        FloatPoint sample = QuadraticBezier(p0, p1, p2, t);
        const float envelope = std::sin(t * kPi);
        sample.x += std::sin(((t * 2.0f) + phase) * kPi) * waveAmplitude * envelope;
        sample.y += std::sin(((t * 1.5f) + phase) * kPi) * 0.35f * envelope;
        RasterizeSegment(previousSample, sample, points);
        previousSample = sample;
    }

    AppendPointIfNeeded(points, endX, endY);
    return points;
}

} // namespace

MapRenderer::MapRenderer(int width, int height)
    : nodes(nullptr),
    cameraY(0.0f),
    targetCameraY(0.0f),
    screenWidth(width),
    screenHeight(height),
    minCameraY(0.0f),
    maxCameraY(0.0f),
    useViewport(false),
    viewportX(0),
    viewportY(0),
    viewportWidth(0),
    viewportHeight(0) {
}

void MapRenderer::RecalculateCameraBounds() {
    if (nodes == nullptr || nodes->empty()) {
        cameraY = 0.0f;
        targetCameraY = 0.0f;
        minCameraY = 0.0f;
        maxCameraY = 0.0f;
        return;
    }

    int minY = nodes->front().y;
    int maxY = nodes->front().y;
    for (const RunNodeState& node : *nodes) {
        minY = (std::min)(minY, node.y);
        maxY = (std::max)(maxY, node.y);
    }

    const int visibleHeight = (useViewport && viewportHeight > 0) ? viewportHeight : screenHeight;
    minCameraY = static_cast<float>(minY - 11);
    maxCameraY = static_cast<float>(maxY - (visibleHeight - 13));
    if (maxCameraY < minCameraY) {
        maxCameraY = minCameraY;
    }

    targetCameraY = (std::max)(minCameraY, (std::min)(maxCameraY, targetCameraY));
    cameraY = (std::max)(minCameraY, (std::min)(maxCameraY, cameraY));
}

void MapRenderer::SetNodes(const std::vector<RunNodeState>* mapNodes) {
    nodes = mapNodes;
    RecalculateCameraBounds();

    if (nodes != nullptr && !nodes->empty()) {
        ResetCameraToBottom();
    }
}

void MapRenderer::SetViewport(int x, int y, int width, int height) {
    useViewport = true;
    viewportX = x;
    viewportY = y;
    viewportWidth = (std::max)(0, width);
    viewportHeight = (std::max)(0, height);
    RecalculateCameraBounds();
}

void MapRenderer::ClearViewport() {
    if (!useViewport) {
        return;
    }

    useViewport = false;
    viewportX = 0;
    viewportY = 0;
    viewportWidth = 0;
    viewportHeight = 0;
    RecalculateCameraBounds();
}

void MapRenderer::ResetCameraToBottom() {
    targetCameraY = maxCameraY;
    cameraY = targetCameraY;
}

void MapRenderer::FocusToFloor(int floor) {
    if (nodes == nullptr || nodes->empty()) {
        return;
    }

    int bestY = nodes->front().y;
    for (const RunNodeState& node : *nodes) {
        if (node.floor == floor) {
            bestY = node.y;
            break;
        }
    }

    const int visibleHeight = (useViewport && viewportHeight > 0) ? viewportHeight : screenHeight;
    targetCameraY = static_cast<float>(bestY - (visibleHeight / 2));
    targetCameraY = (std::max)(minCameraY, (std::min)(maxCameraY, targetCameraY));
}

std::string MapRenderer::GetNodeLabel(RunNodeType type) const {
    switch (type) {
    case RunNodeType::Battle:   return u8"전";
    case RunNodeType::Elite:    return u8"엘";
    case RunNodeType::Boss:     return u8"왕";
    case RunNodeType::Shop:     return u8"상";
    case RunNodeType::Rest:     return u8"휴";
    case RunNodeType::Treasure: return u8"보";
    case RunNodeType::Event:    return u8"미";
    default:                    return u8"?";
    }
}

int MapRenderer::GetNodeWidth() const {
    return 5;
}

int MapRenderer::GetNodeHeight() const {
    return 3;
}

WORD MapRenderer::GetNodeColor(const RunNodeState& node) const {
    if (node.isCurrent) {
        return COLOR_YELLOW;
    }
    if (node.completed) {
        return FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    }
    if (!node.reachable) {
        return FOREGROUND_INTENSITY;
    }

    switch (node.type) {
    case RunNodeType::Battle:   return COLOR_WHITE;
    case RunNodeType::Elite:    return COLOR_RED;
    case RunNodeType::Boss:     return COLOR_RED | FOREGROUND_INTENSITY;
    case RunNodeType::Shop:     return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    case RunNodeType::Rest:     return COLOR_GREEN;
    case RunNodeType::Treasure: return COLOR_YELLOW;
    case RunNodeType::Event:    return FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    default:                    return COLOR_WHITE;
    }
}

WORD MapRenderer::GetPathColor(const RunNodeState& fromNode, const RunNodeState& toNode) const {
    return FOREGROUND_INTENSITY;
}

void MapRenderer::DrawPath(ScreenManager& screen, int x1, int y1, int x2, int y2, WORD color, int hashValue) const {
    const RectI clipRect = useViewport
        ? RectI{ viewportX, viewportY, viewportWidth, viewportHeight }
        : RectI{ 0, 0, screenWidth, screenHeight };

    const int halfNodeHeight = GetNodeHeight() / 2;
    const int startX = x1;
    const int startY = y1 - halfNodeHeight - 1;
    const int endX = x2;
    const int endY = y2 + halfNodeHeight + 1;
    if (startY <= endY) {
        return;
    }

    const int totalDx = std::abs(endX - startX);
    const int totalRows = startY - endY;
    const std::vector<IntPoint> pathPoints = BuildPathPoints(startX, startY, endX, endY, hashValue);
    if (pathPoints.empty()) {
        return;
    }

    for (size_t index = 0; index < pathPoints.size(); ++index) {
        const IntPoint* previous = (index > 0) ? &pathPoints[index - 1] : nullptr;
        const IntPoint* next = (index + 1 < pathPoints.size()) ? &pathPoints[index + 1] : nullptr;
        const IntPoint& point = pathPoints[index];
        const int renderY = point.y - static_cast<int>(cameraY);

        wchar_t glyph = L'|';
        if (next != nullptr) {
            glyph = SelectPathGlyph(previous, point, *next);
        }
        else if (previous != nullptr) {
            glyph = SelectPathGlyph(nullptr, *previous, point);
        }

        DrawCharClipped(screen, point.x, renderY, glyph, color, clipRect);

        if (glyph == L'╱' || glyph == L'/' || glyph == L'╲' || glyph == L'\\') {
            int assistDirection = 0;
            if (next != nullptr) {
                assistDirection = next->x - point.x;
            }
            else if (previous != nullptr) {
                assistDirection = point.x - previous->x;
            }

            const bool wideDiagonal = totalDx >= (std::max)(10, static_cast<int>(std::ceil(static_cast<float>(totalRows) * 0.6f)));
            const int assistSpan = wideDiagonal ? 2 : 1;
            if (assistDirection > 0) {
                for (int offset = 1; offset <= assistSpan; ++offset) {
                    DrawCharClipped(screen, point.x + offset, renderY, L'/', color, clipRect);
                }
            }
            else if (assistDirection < 0) {
                for (int offset = 1; offset <= assistSpan; ++offset) {
                    DrawCharClipped(screen, point.x - offset, renderY, L'\\', color, clipRect);
                }
            }
        }
    }
}

void MapRenderer::Update(InputManager& input) {
    const int wheel = input.GetWheelDelta();
    if (wheel != 0) {
        targetCameraY -= static_cast<float>(wheel * 4);
    }

    targetCameraY = (std::max)(minCameraY, (std::min)(maxCameraY, targetCameraY));
    cameraY += (targetCameraY - cameraY) * 0.2f;
}

void MapRenderer::Render(ScreenManager& screen) const {
    if (nodes == nullptr) {
        return;
    }

    const RectI clipRect = useViewport
        ? RectI{ viewportX, viewportY, viewportWidth, viewportHeight }
        : RectI{ 0, 0, screenWidth, screenHeight };

    for (const RunNodeState& node : *nodes) {
        for (int nextId : node.nextNodeIds) {
            const RunNodeState* nextNode = nullptr;
            for (const RunNodeState& candidate : *nodes) {
                if (candidate.id == nextId) {
                    nextNode = &candidate;
                    break;
                }
            }

            if (nextNode != nullptr) {
                const int hashValue = (node.id * 131) ^ (nextNode->id * 17) ^ (node.floor * 73);
                DrawPath(screen, node.x, node.y, nextNode->x, nextNode->y, GetPathColor(node, *nextNode), hashValue);
            }
        }
    }

    const int nodeWidth = GetNodeWidth();
    const int nodeHeight = GetNodeHeight();

    for (const RunNodeState& node : *nodes) {
        const int renderY = node.y - static_cast<int>(cameraY);
        const int boxX = node.x - (nodeWidth / 2);
        const int boxY = renderY - 1;
        if (!IsRectVisible(boxX, boxY, nodeWidth, nodeHeight, clipRect)) {
            continue;
        }

        const WORD color = GetNodeColor(node);
        const std::wstring label = TextLayout::AlignToWidth(
            TextLayout::Utf8ToWide(GetNodeLabel(node.type)),
            nodeWidth - 2,
            TextLayout::HorizontalAlign::Center);

        DrawCharClipped(screen, boxX, boxY, L'+', color, clipRect);
        DrawCharClipped(screen, boxX + nodeWidth - 1, boxY, L'+', color, clipRect);
        DrawCharClipped(screen, boxX, boxY + 1, L'|', color, clipRect);
        DrawCharClipped(screen, boxX + nodeWidth - 1, boxY + 1, L'|', color, clipRect);
        DrawCharClipped(screen, boxX, boxY + 2, L'+', color, clipRect);
        DrawCharClipped(screen, boxX + nodeWidth - 1, boxY + 2, L'+', color, clipRect);

        for (int offset = 1; offset < nodeWidth - 1; ++offset) {
            DrawCharClipped(screen, boxX + offset, boxY, L'-', color, clipRect);
            DrawCharClipped(screen, boxX + offset, boxY + 2, L'-', color, clipRect);
        }

        DrawWideTextClipped(screen, boxX + 1, boxY + 1, label, color, clipRect);
    }
}

bool MapRenderer::TryGetNodeTooltip(int mouseX, int mouseY, std::vector<std::string>& outLines) const {
    int nodeId = -1;
    if (!TryGetHoveredNodeId(mouseX, mouseY, nodeId) || nodes == nullptr) {
        outLines.clear();
        return false;
    }

    for (const RunNodeState& node : *nodes) {
        if (node.id != nodeId) {
            continue;
        }

        std::string statusText = u8"잠김";
        if (node.completed) {
            statusText = u8"완료됨";
        }
        else if (node.reachable) {
            statusText = u8"이동 가능";
        }
        else if (node.unlocked) {
            statusText = u8"도달 대기";
        }

        outLines = {
            RunNodeTypeToDisplayName(node.type),
            RunNodeTypeToDescription(node.type),
            std::string(u8"층 ") + std::to_string(node.floor),
            statusText
        };
        return true;
    }

    outLines.clear();
    return false;
}

bool MapRenderer::TryGetHoveredNodeId(int mouseX, int mouseY, int& outNodeId) const {
    outNodeId = -1;
    if (nodes == nullptr) {
        return false;
    }

    if (useViewport) {
        const RectI clipRect = { viewportX, viewportY, viewportWidth, viewportHeight };
        if (!IsInsideRect(mouseX, mouseY, clipRect)) {
            return false;
        }
    }

    for (const RunNodeState& node : *nodes) {
        const int renderY = node.y - static_cast<int>(cameraY);
        const int boxX = node.x - (GetNodeWidth() / 2);
        const int boxY = renderY - 1;

        if (mouseX >= boxX &&
            mouseX < boxX + GetNodeWidth() &&
            mouseY >= boxY &&
            mouseY < boxY + GetNodeHeight()) {
            outNodeId = node.id;
            return true;
        }
    }

    return false;
}
