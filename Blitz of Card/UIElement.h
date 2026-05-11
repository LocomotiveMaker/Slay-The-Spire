// -----------------------------------------------------------------------------
// @file       UIElement.h
// @brief      모든 UI 컴포넌트의 추상 부모 클래스
// -----------------------------------------------------------------------------
#pragma once
#include "ScreenManager.h"
#include "InputManager.h"

class UIElement {
protected:
    int x, y;
    int width, height;
    bool isHovered;

public:
    UIElement(int x, int y, int w, int h) : x(x), y(y), width(w), height(h), isHovered(false) {}
    virtual ~UIElement() = default;

    // [추가] 모든 UI 요소가 공통으로 사용할 AABB 충돌 판정 메서드
    bool IsPointInside(int targetX, int targetY) const {
        return (targetX >= x && targetX < x + width && targetY >= y && targetY < y + height);
    }

    // 마우스 입력을 받아 상태를 갱신
    // 반환값: 이 UI가 마우스 이벤트를 소비 했는지 여부
    virtual bool Update(InputManager& input) {
        int mx = input.GetMouseX();
        int my = input.GetMouseY();

        // AABB(사각형) 충돌 판정
        if (mx >= x && mx < x + width && my >= y && my < y + height) {
            isHovered = true;
            return true; // 마우스 닿으면 밑에 깔린 애들 무시
        }
        isHovered = false;
        return false;
    }

    virtual void Render(ScreenManager& screen) = 0;

    // Getters & Setters
    bool IsHovered() const { return isHovered; }
    void SetHovered(bool state) { isHovered = state; } // 강제 해제용
    void SetPosition(int newX, int newY) { x = newX; y = newY; }
};