// -----------------------------------------------------------------------------
// @file       MapRenderer.cpp
// -----------------------------------------------------------------------------
#include "MapRenderer.h"
#include <algorithm>
#include <cmath>

namespace {

std::string GetNodeTitle(NodeType type) {
    switch (type) {
    case NodeType::Monster: return u8"전투";
    case NodeType::Elite:   return u8"정예";
    case NodeType::Rest:    return u8"휴식";
    case NodeType::Shop:    return u8"상점";
    case NodeType::Unknown: return u8"미지";
    case NodeType::Boss:    return u8"보스";
    default:                return u8"노드";
    }
}

std::string GetNodeDescription(NodeType type) {
    switch (type) {
    case NodeType::Monster: return u8"일반 적과 전투합니다.";
    case NodeType::Elite:   return u8"강한 적과 전투하고, 유물을 얻습니다.";
    case NodeType::Rest:    return u8"체력을 회복하거나, 카드를 강화합니다.";
    case NodeType::Shop:    return u8"골드로 카드, 포션, 유물을 구매합니다.";
    case NodeType::Unknown: return u8"이벤트가 발생하며, 선택에 따라 결과가 바뀝니다.";
    case NodeType::Boss:    return u8"보스전입니다. 전시의 핵심 전투가 기다립니다.";
    default:                return u8"설명이 준비되지 않았습니다.";
    }
}

} // namespace

MapRenderer::MapRenderer(int width, int height)
    : screenWidth(width), screenHeight(height), cameraY(0.0f), targetCameraY(0.0f), maxMapHeight(0), minCameraY(0.0f), maxCameraY(0.0f) {
}

void MapRenderer::GenerateDummyMap() {
    nodes.clear();
    int startX = screenWidth / 2;
    int startY = screenHeight - 15;
    int ySpacing = 20;

    nodes.push_back({ 0, startX - 25, startY, NodeType::Monster, {3, 4} });
    nodes.push_back({ 1, startX,      startY, NodeType::Monster, {4} });
    nodes.push_back({ 2, startX + 25, startY, NodeType::Unknown, {4, 5} });

    nodes.push_back({ 3, startX - 30, startY - ySpacing, NodeType::Unknown, {6} });
    nodes.push_back({ 4, startX,      startY - ySpacing, NodeType::Shop,    {6, 7} });
    nodes.push_back({ 5, startX + 30, startY - ySpacing, NodeType::Monster, {7} });

    nodes.push_back({ 6, startX - 15, startY - ySpacing * 2, NodeType::Elite, {8} });
    nodes.push_back({ 7, startX + 15, startY - ySpacing * 2, NodeType::Rest,  {8} });

    nodes.push_back({ 8, startX,      startY - ySpacing * 3, NodeType::Boss,  {} });

    maxMapHeight = ySpacing * 3;
    minCameraY = static_cast<float>((startY - maxMapHeight) - 20);
    maxCameraY = 20.0f;
    cameraY = maxCameraY;
    targetCameraY = maxCameraY;
}

std::string MapRenderer::GetNodeIcon(NodeType type) {
    switch (type) {
    case NodeType::Monster: return " M ";
    case NodeType::Elite:   return " E ";
    case NodeType::Rest:    return " R ";
    case NodeType::Shop:    return " $ ";
    case NodeType::Unknown: return " ? ";
    case NodeType::Boss:    return "BOSS";
    default:                return "   ";
    }
}

WORD MapRenderer::GetNodeColor(NodeType type) {
    switch (type) {
    case NodeType::Monster: return COLOR_WHITE;
    case NodeType::Elite:   return FOREGROUND_RED | FOREGROUND_INTENSITY;
    case NodeType::Rest:    return FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    case NodeType::Shop:    return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    case NodeType::Unknown: return FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    case NodeType::Boss:    return FOREGROUND_RED | FOREGROUND_INTENSITY;
    default:                return COLOR_WHITE;
    }
}

void MapRenderer::DrawPath(ScreenManager& screen, int x1, int y1, int x2, int y2) {
    int dx = std::abs(x2 - x1);
    int dy = std::abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    int skipMargin = 5;

    while (true) {
        if (x1 == x2 && y1 == y2) break;

        int renderY = y1 - static_cast<int>(cameraY);
        bool isOutsideMargin =
            (std::abs(x1 - x2) + std::abs(y1 - y2) > skipMargin) &&
            (std::abs(x1 - (x2 - dx * sx)) + std::abs(y1 - (y2 - dy * sy)) > skipMargin);

        if (renderY >= 0 && renderY < screenHeight && isOutsideMargin) {
            char pathChar = '.';
            if (dx > dy * 1.5f) pathChar = (sx * sy > 0) ? '\\' : '/';
            else if (dy > dx * 2) pathChar = '|';

            if ((x1 + y1) % 2 == 0) {
                screen.DrawChar(x1, renderY, pathChar, FOREGROUND_INTENSITY);
                screen.DrawChar(x1 + 1, renderY, pathChar, FOREGROUND_INTENSITY);
            }
        }

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

void MapRenderer::Update(InputManager& input) {
    const float scrollSpeed = 5.0f;
    const int wheel = input.GetWheelDelta();

    if (wheel != 0) {
        targetCameraY -= wheel * scrollSpeed;
    }

    if (targetCameraY < minCameraY) targetCameraY = minCameraY;
    if (targetCameraY > maxCameraY) targetCameraY = maxCameraY;

    cameraY += (targetCameraY - cameraY) * 0.15f;
}

void MapRenderer::Render(ScreenManager& screen) {
    for (const auto& node : nodes) {
        for (int nextId : node.nextNodes) {
            auto it = std::find_if(nodes.begin(), nodes.end(), [nextId](const MapNode& nextNode) {
                return nextNode.id == nextId;
            });
            if (it != nodes.end()) {
                DrawPath(screen, node.x, node.y, it->x, it->y);
            }
        }
    }

    for (const auto& node : nodes) {
        const int renderY = node.y - static_cast<int>(cameraY);
        if (renderY < -3 || renderY > screen.GetHeight()) continue;

        const std::string icon = GetNodeIcon(node.type);
        const WORD color = GetNodeColor(node.type);
        const int boxX = node.x - 2;
        const int boxY = renderY - 1;

        screen.DrawString(boxX, boxY, "-----", color);
        screen.DrawString(boxX, boxY + 1, "|" + icon + "|", color);
        screen.DrawString(boxX, boxY + 2, "-----", color);
    }
}

bool MapRenderer::TryGetNodeTooltip(int mouseX, int mouseY, std::vector<std::string>& outLines) const {
    for (const auto& node : nodes) {
        const int renderY = node.y - static_cast<int>(cameraY);
        const int boxX = node.x - 2;
        const int boxY = renderY - 1;

        if (mouseX >= boxX && mouseX < boxX + 5 && mouseY >= boxY && mouseY < boxY + 3) {
            outLines = {
                GetNodeTitle(node.type),
                GetNodeDescription(node.type)
            };
            return true;
        }
    }

    outLines.clear();
    return false;
}
