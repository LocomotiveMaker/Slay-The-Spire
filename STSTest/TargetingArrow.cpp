// -----------------------------------------------------------------------------
// @file       TargetingArrow.cpp
// -----------------------------------------------------------------------------
#include "TargetingArrow.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TargetingArrow::TargetingArrow() : startX(0), startY(0), endX(0), endY(0), isActive(false) {}

void TargetingArrow::SetStartPoint(int x, int y) {
    startX = x;
    startY = y;
}

void TargetingArrow::SetEndPoint(int x, int y) {
    endX = x;
    endY = y;
}

void TargetingArrow::SetActive(bool state) {
    isActive = state;
}

bool TargetingArrow::IsActive() const {
    return isActive;
}

void TargetingArrow::DrawArrowHead(ScreenManager& screen, int x, int y, double angle) {
    // angle은 라디안 단위. -PI ~ PI 사이의 값.
    // 각도를 디그리(Degree)로 변환하여 5방향(좌, 좌상, 상, 우상, 우) 판별
    // 카드는 보통 화면 아래에 있으므로 아래 방향은 제외하거나 단순화함.
    double deg = angle * (180.0 / M_PI);

    WORD color = COLOR_RED; // 또는 FOREGROUND_RED | FOREGROUND_INTENSITY 등 눈에 띄는 색상

    // 슬더스의 굵은 화살표 느낌을 내기 위해 여러 문자를 조합해 그립니다.
    if (deg > -22.5 && deg <= 22.5) {
        // [오른쪽 방향] (0도 부근)
        screen.DrawString(x - 2, y - 1, "\\\\", color);
        screen.DrawString(x - 1, y, "==>", color);
        screen.DrawString(x - 2, y + 1, "//", color);
    }
    else if (deg > 22.5 && deg <= 67.5) {
        // [우측 하단 방향] -> 실제로는 안 쓰이지만 범위 맞춤용
        screen.DrawString(x, y, ">", color);
    }
    else if (deg > 67.5 && deg <= 112.5) {
        // [아래 방향] -> 역시 카드 사용 시엔 드물게 쓰임
        screen.DrawString(x, y, "v", color);
    }
    else if (deg > 112.5 && deg <= 157.5) {
        //[좌측 하단 방향]
        screen.DrawString(x, y, "<", color);
    }
    else if (deg > 157.5 || deg <= -157.5) {
        //[왼쪽 방향] (+-180도 부근)
        screen.DrawString(x, y - 1, "//", color);
        screen.DrawString(x - 1, y, "<==", color);
        screen.DrawString(x, y + 1, "\\\\", color);
    }
    else if (deg > -157.5 && deg <= -112.5) {
        //[좌측 상단 방향]
        screen.DrawString(x - 2, y - 2, "<\\\\", color);
        screen.DrawString(x - 1, y - 1, " \\\\", color);
        screen.DrawString(x, y, "  \\\\", color);
    }
    else if (deg > -112.5 && deg <= -67.5) {
        // [위쪽 방향] (-90도 부근)
        screen.DrawString(x - 1, y - 2, "/^\\", color);
        screen.DrawString(x - 1, y - 1, "|||", color);
        screen.DrawString(x - 1, y, "|||", color);
    }
    else if (deg > -67.5 && deg <= -22.5) {
        // [우측 상단 방향]
        screen.DrawString(x, y - 2, "//>", color);
        screen.DrawString(x - 1, y - 1, "// ", color);
        screen.DrawString(x - 2, y, "//  ", color);
    }
}

void TargetingArrow::Render(ScreenManager& screen) {
    if (!isActive) return;

    // 시작점과 끝점이 너무 가까우면 그리지 않음
    if (std::abs(endX - startX) < 3 && std::abs(endY - startY) < 3) return;

    // 1. 2차 베지에 곡선 제어점(Control Point) 설정
    // 슬더스 특유의 낭창낭창한 느낌을 내기 위해, 시작점과 끝점 사이의 중간 지점에서 
    // 위쪽(또는 옆쪽)으로 살짝 띄운 좌표를 제어점으로 잡습니다.
    int cpX = startX + (endX - startX) / 2;
    int cpY = min(startY, endY) - 10; // 곡선이 위로 살짝 휘어지도록 만듦

    // 선을 이루는 점의 개수 (해상도에 따라 조절)
    int steps = 25;

    // 이전 점 좌표 기억 (각도 계산용)
    int prevX = startX;
    int prevY = startY;
    int currentX = startX;
    int currentY = startY;

    // 2. 곡선 렌더링 및 두께 증가
    for (int i = 1; i <= steps; ++i) {
        double t = (double)i / steps;

        // 베지에 곡선 공식: B(t) = (1-t)^2*P0 + 2(1-t)t*P1 + t^2*P2
        double u = 1.0 - t;
        double tt = t * t;
        double uu = u * u;

        currentX = (int)(uu * startX + 2 * u * t * cpX + tt * endX);
        currentY = (int)(uu * startY + 2 * u * t * cpY + tt * endY);

        // 두께 조절 로직 (t가 1에 가까울수록, 즉 끝점에 갈수록 두꺼워짐)
        // t 값에 따라 주변에 찍는 점의 범위를 늘립니다.
        int thickness = (int)(t * 3); // 끝부분에서 최대 2칸 반경까지 칠함

        // 화살표 머리와 겹치지 않게 마지막 몇 스텝은 선을 그리지 않음
        if (i < steps - 1) {
            for (int dy = -thickness; dy <= thickness; ++dy) {
                for (int dx = -thickness; dx <= thickness; ++dx) {
                    // 원형으로 두께를 주기 위한 단순 거리 체크
                    if (dx * dx + dy * dy <= thickness * thickness) {
                        screen.DrawChar(currentX + dx, currentY + dy, '#', COLOR_RED); // 선 색상 및 문자
                    }
                }
            }
        }

        // 끝점 바로 직전의 좌표를 저장해 둠 (마지막에 각도 계산에 사용)
        if (i == steps - 1) {
            prevX = currentX;
            prevY = currentY;
        }
    }

    // 3. 끝점(마우스 위치)에 화살표 머리 그리기
    // 끝점과 그 직전 점 사이의 각도 계산 (atan2 사용)
    double angle = std::atan2(endY - prevY, endX - prevX);
    DrawArrowHead(screen, endX, endY, angle);
}