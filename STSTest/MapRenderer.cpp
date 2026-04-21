// -----------------------------------------------------------------------------
// @file       MapRenderer.cpp
// -----------------------------------------------------------------------------
#include "MapRenderer.h"
#include <cmath>
#include <algorithm>

MapRenderer::MapRenderer(int width, int height)
    : screenWidth(width), screenHeight(height), cameraY(0.0f), targetCameraY(0.0f) {
}

void MapRenderer::GenerateDummyMap() {
    nodes.clear();
    int startX = screenWidth / 2;
    int startY = screenHeight - 15; // 뷰포트 하단 근처
    int ySpacing = 20; // 층 간격

    nodes.push_back({ 0, startX - 25, startY, NodeType::Monster, {3, 4} });
    nodes.push_back({ 1, startX,      startY, NodeType::Monster, {4} });
    nodes.push_back({ 2, startX + 25, startY, NodeType::Unknown, {4, 5} });

    nodes.push_back({ 3, startX - 30, startY - ySpacing, NodeType::Unknown, {6} });
    nodes.push_back({ 4, startX,      startY - ySpacing, NodeType::Shop,    {6, 7} });
    nodes.push_back({ 5, startX + 30, startY - ySpacing, NodeType::Monster, {7} });

    nodes.push_back({ 6, startX - 15, startY - ySpacing * 2, NodeType::Elite, {8} });
    nodes.push_back({ 7, startX + 15, startY - ySpacing * 2, NodeType::Rest,  {8} });

    nodes.push_back({ 8, startX,      startY - ySpacing * 3, NodeType::Boss,  {} });

    // 카메라 스크롤 한계값 동적 계산
    // boss.y가 화면 상단(예: y=10)에 오도록 하는 cameraY 값
    minCameraY = (startY - ySpacing * 3) - 20;
    // bottom node가 화면 하단에 오도록 하는 cameraY 값 (초기 위치)
    maxCameraY = 20.0f;

    // 초기 카메라 위치 세팅
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

    // 노드 박스 크기(5x3)를 고려하여 중심부터 일정 거리(반지름 약 4)는 그리지 않음
    int skipMargin = 5;

    while (true) {
        if (x1 == x2 && y1 == y2) break;

        int renderY = y1 - (int)cameraY;

        // 시작점과 끝점 근처는 그리지 않음 (노드 아트를 침범하지 않게)
        bool isOutsideMargin = (std::abs(x1 - x2) + std::abs(y1 - y2) > skipMargin) &&
            (std::abs(x1 - (x2 - dx * sx)) + std::abs(y1 - (y2 - dy * sy)) > skipMargin);

        if (renderY >= 0 && renderY < screenHeight && isOutsideMargin) {
            char pathChar = '.';
            if (dx > dy * 1.5) pathChar = (sx * sy > 0) ? '\\' : '/';
            else if (dy > dx * 2) pathChar = '|';

            // 경로를 굵게 보이기 위해 가로로 2칸씩 렌더링
            if ((x1 + y1) % 2 == 0) {
                screen.DrawChar(x1, renderY, pathChar, FOREGROUND_INTENSITY);
                screen.DrawChar(x1 + 1, renderY, pathChar, FOREGROUND_INTENSITY); // 두께 추가
            }
        }

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

void MapRenderer::Update(InputManager& input) {
    float scrollSpeed = 5.0f;
    int wheel = input.GetWheelDelta();

    if (wheel != 0) {
        targetCameraY -= wheel * scrollSpeed;
    }

    // 카메라 스크롤 한계 적용 (Clamping)
    if (targetCameraY < minCameraY) targetCameraY = minCameraY;
    if (targetCameraY > maxCameraY) targetCameraY = maxCameraY;

    cameraY += (targetCameraY - cameraY) * 0.15f;
}

void MapRenderer::Render(ScreenManager& screen) {
    // 경로 렌더링
    for (const auto& node : nodes) {
        for (int nextId : node.nextNodes) {
            auto it = std::find_if(nodes.begin(), nodes.end(), [nextId](const MapNode& n) { return n.id == nextId; });
            if (it != nodes.end()) {
                DrawPath(screen, node.x, node.y, it->x, it->y);
            }
        }
    }

    // 노드 박스 렌더링 (5x3 크기의 사각형)
    for (const auto& node : nodes) {
        int renderY = node.y - (int)cameraY;
        if (renderY < -3 || renderY > screen.GetHeight()) continue; // 화면 밖 Culling

        std::string icon = GetNodeIcon(node.type);
        WORD color = GetNodeColor(node.type);

        // 5x3 박스 렌더링 (중심 정렬)
        int boxX = node.x - 2;
        int boxY = renderY - 1;

        screen.DrawString(boxX, boxY, "-----", color);
        screen.DrawString(boxX, boxY + 1, "|" + icon + "|", color);
        screen.DrawString(boxX, boxY + 2, "-----", color);
    }
}