// -----------------------------------------------------------------------------
// @file       FloatingText.h
// @brief      데미지 표기용 플로팅 텍스트 렌더링 클래스
// -----------------------------------------------------------------------------
#pragma once
#include "ScreenManager.h"
#include <string>

class FloatingText {
private:
    double x;
    double y;
    std::string text;
    WORD color;
    int lifetime;    // 남은 출력 프레임
    int maxLifetime;

public:
    FloatingText(double startX, double startY, const std::string& text, WORD color)
        : x(startX), y(startY), text(text), color(color), lifetime(60), maxLifetime(60) {
    }

    // 상태 갱신. 수명이 다하면 false 반환
    bool Update() {
        if (lifetime <= 0) return false;

        // 매 프레임 Y 좌표를 미세하게 감소시켜 위로 떠오르는 효과 구현
        y -= 0.05;
        lifetime--;
        return true;
    }

    void Render(ScreenManager& screen) {
        if (lifetime > 0) {
            screen.DrawString((int)x, (int)y, text, color);
        }
    }
};