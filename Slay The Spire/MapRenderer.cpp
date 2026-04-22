// -----------------------------------------------------------------------------
// @file       MapRenderer.cpp
// -----------------------------------------------------------------------------
#include "MapRenderer.h"
#include <algorithm>
#include <cmath>

MapRenderer::MapRenderer(int width, int height)
    : nodes(nullptr),
    cameraY(0.0f),
    targetCameraY(0.0f),
    screenWidth(width),
    screenHeight(height),
    minCameraY(0.0f),
    maxCameraY(0.0f) {
}

void MapRenderer::SetNodes(const std::vector<RunNodeState>* mapNodes) {
    nodes = mapNodes;

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

    minCameraY = static_cast<float>(minY - 8);
    maxCameraY = static_cast<float>(maxY - (screenHeight - 12));

    if (maxCameraY < minCameraY) {
        maxCameraY = minCameraY;
    }

    ResetCameraToBottom();
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

    targetCameraY = static_cast<float>(bestY - (screenHeight / 2));
    if (targetCameraY < minCameraY) targetCameraY = minCameraY;
    if (targetCameraY > maxCameraY) targetCameraY = maxCameraY;
}

std::string MapRenderer::GetNodeIcon(RunNodeType type) const {
    switch (type) {
    case RunNodeType::Battle:   return "M";
    case RunNodeType::Elite:    return "E";
    case RunNodeType::Boss:     return "B";
    case RunNodeType::Shop:     return "$";
    case RunNodeType::Rest:     return "R";
    case RunNodeType::Treasure: return "T";
    case RunNodeType::Event:    return "?";
    default:                    return ".";
    }
}

WORD MapRenderer::GetNodeColor(const RunNodeState& node) const {
    if (node.isCurrent) {
        return COLOR_YELLOW;
    }

    if (node.completed) {
        return FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    }

    if (!node.unlocked) {
        return FOREGROUND_INTENSITY;
    }

    switch (node.type) {
    case RunNodeType::Battle:   return COLOR_WHITE;
    case RunNodeType::Elite:    return COLOR_RED;
    case RunNodeType::Boss:     return COLOR_RED;
    case RunNodeType::Shop:     return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    case RunNodeType::Rest:     return COLOR_GREEN;
    case RunNodeType::Treasure: return COLOR_YELLOW;
    case RunNodeType::Event:    return FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    default:                    return COLOR_WHITE;
    }
}

void MapRenderer::DrawPath(ScreenManager& screen, int x1, int y1, int x2, int y2, WORD color) const {
    int dx = std::abs(x2 - x1);
    int dy = std::abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        const int renderY = y1 - static_cast<int>(cameraY);
        if (renderY >= 0 && renderY < screenHeight && x1 >= 0 && x1 < screenWidth) {
            char pathChar = '.';
            if (dx > dy * 2) pathChar = '-';
            else if (dy > dx * 2) pathChar = '|';
            else pathChar = (sx * sy > 0) ? '\\' : '/';
            screen.DrawChar(x1, renderY, pathChar, color);
        }

        if (x1 == x2 && y1 == y2) {
            break;
        }

        const int e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void MapRenderer::Update(InputManager& input) {
    const int wheel = input.GetWheelDelta();
    if (wheel != 0) {
        targetCameraY -= static_cast<float>(wheel * 4);
    }

    if (targetCameraY < minCameraY) targetCameraY = minCameraY;
    if (targetCameraY > maxCameraY) targetCameraY = maxCameraY;

    cameraY += (targetCameraY - cameraY) * 0.2f;
}

void MapRenderer::Render(ScreenManager& screen) const {
    if (nodes == nullptr) {
        return;
    }

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
                DrawPath(screen, node.x, node.y, nextNode->x, nextNode->y, FOREGROUND_INTENSITY);
            }
        }
    }

    for (const RunNodeState& node : *nodes) {
        const int renderY = node.y - static_cast<int>(cameraY);
        if (renderY < -1 || renderY >= screen.GetHeight() - 1) {
            continue;
        }

        const WORD color = GetNodeColor(node);
        const int boxX = node.x - 2;
        screen.DrawString(boxX, renderY - 1, ".---.", color);
        screen.DrawString(boxX, renderY + 0, "|" + std::string(1, GetNodeIcon(node.type)[0]) + std::string(1, node.completed ? '*' : ' ') + "|", color);
        screen.DrawString(boxX, renderY + 1, "'---'", color);
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

        outLines = {
            RunNodeTypeToDisplayName(node.type),
            RunNodeTypeToDescription(node.type),
            std::string(u8"층 ") + std::to_string(node.floor),
            node.completed ? u8"해결됨" : (node.unlocked ? u8"진입 가능" : u8"잠김")
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

    for (const RunNodeState& node : *nodes) {
        const int renderY = node.y - static_cast<int>(cameraY);
        const int boxX = node.x - 2;
        if (mouseX >= boxX && mouseX < boxX + 5 && mouseY >= renderY - 1 && mouseY < renderY + 2) {
            outNodeId = node.id;
            return true;
        }
    }

    return false;
}
