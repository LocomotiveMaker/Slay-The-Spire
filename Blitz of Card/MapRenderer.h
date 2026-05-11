// -----------------------------------------------------------------------------
// @file       MapRenderer.h
// @brief      런 지도 렌더링과 스크롤 제어 인터페이스
// -----------------------------------------------------------------------------
#pragma once
#include "InputManager.h"
#include "RunState.h"
#include "ScreenManager.h"
#include "TextLayout.h"
#include <string>
#include <vector>

class MapRenderer {
private:
    const std::vector<RunNodeState>* nodes;
    float cameraY;
    float targetCameraY;
    int screenWidth;
    int screenHeight;
    float minCameraY;
    float maxCameraY;
    bool useViewport;
    int viewportX;
    int viewportY;
    int viewportWidth;
    int viewportHeight;

    void RecalculateCameraBounds();
    void DrawPath(ScreenManager& screen, int x1, int y1, int x2, int y2, WORD color, int hashValue) const;
    std::string GetNodeLabel(RunNodeType type) const;
    WORD GetNodeColor(const RunNodeState& node) const;
    WORD GetPathColor(const RunNodeState& fromNode, const RunNodeState& toNode) const;
    int GetNodeWidth() const;
    int GetNodeHeight() const;

public:
    MapRenderer(int width, int height);

    void SetNodes(const std::vector<RunNodeState>* mapNodes);
    void SetViewport(int x, int y, int width, int height);
    void ClearViewport();
    void ResetCameraToBottom();
    void FocusToFloor(int floor);

    void Update(InputManager& input);
    void Render(ScreenManager& screen) const;
    bool TryGetNodeTooltip(int mouseX, int mouseY, std::vector<std::string>& outLines) const;
    bool TryGetHoveredNodeId(int mouseX, int mouseY, int& outNodeId) const;
};
