// -----------------------------------------------------------------------------
// @file       TargetingArrow.h
// @brief      베지에 곡선 기반 동적 두께 타겟팅 화살표 렌더링 클래스
// -----------------------------------------------------------------------------
#pragma once
#include "ScreenManager.h"

class TargetingArrow {
private:
    int startX;
    int startY;
    int endX;
    int endY;
    bool isActive;

    // 내부 헬퍼 함수: 각도에 따른 화살표 머리 그리기
    void DrawArrowHead(ScreenManager& screen, int x, int y, double angle);

public:
    TargetingArrow();

    void SetStartPoint(int x, int y);
    void SetEndPoint(int x, int y);
    void SetActive(bool state);
    bool IsActive() const;

    void Render(ScreenManager& screen);
};