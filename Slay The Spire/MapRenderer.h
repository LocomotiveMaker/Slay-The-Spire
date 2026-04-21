// -----------------------------------------------------------------------------
// @file       MapRenderer.h
// @brief      지도 렌더링 및 휠 스크롤 카메라 제어 클래스
// -----------------------------------------------------------------------------
#pragma once
#include "ScreenManager.h"
#include "InputManager.h"
#include <vector>
#include <string>

enum class NodeType { Monster, Elite, Rest, Shop, Unknown, Boss };

struct MapNode {
    int id;
    int x;
    int y;
    NodeType type;
    std::vector<int> nextNodes; // 상위 층으로 연결된 노드의 인덱스
};

class MapRenderer {
private:
    std::vector<MapNode> nodes;

    float cameraY;
    float targetCameraY; // 부드러운 스크롤링(Lerp)을 위한 목표 위치

    int screenWidth;
    int screenHeight;

    int maxMapHeight; // 지도의 총 세로 길이
    float minCameraY; // 스크롤 상단 한계 (보스 노드 쪽)
    float maxCameraY; // 스크롤 하단 한계 (시작 노드 쪽)

    // 헬퍼 메서드
    void DrawPath(ScreenManager& screen, int x1, int y1, int x2, int y2);
    std::string GetNodeIcon(NodeType type);
    WORD GetNodeColor(NodeType type);

public:
    MapRenderer(int width, int height);

    void GenerateDummyMap();
    void Update(InputManager& input);
    void Render(ScreenManager& screen);
    bool TryGetNodeTooltip(int mouseX, int mouseY, std::vector<std::string>& outLines) const;
};
