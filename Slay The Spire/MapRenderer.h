// -----------------------------------------------------------------------------
// @file       MapRenderer.h
// @brief      Run map rendering and scroll control.
// -----------------------------------------------------------------------------
#pragma once
#include "InputManager.h"
#include "RunState.h"
#include "ScreenManager.h"
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

    void DrawPath(ScreenManager& screen, int x1, int y1, int x2, int y2, WORD color) const;
    std::string GetNodeIcon(RunNodeType type) const;
    WORD GetNodeColor(const RunNodeState& node) const;

public:
    MapRenderer(int width, int height);

    void SetNodes(const std::vector<RunNodeState>* mapNodes);
    void ResetCameraToBottom();
    void FocusToFloor(int floor);

    void Update(InputManager& input);
    void Render(ScreenManager& screen) const;
    bool TryGetNodeTooltip(int mouseX, int mouseY, std::vector<std::string>& outLines) const;
    bool TryGetHoveredNodeId(int mouseX, int mouseY, int& outNodeId) const;
};
