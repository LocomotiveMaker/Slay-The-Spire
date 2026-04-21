#pragma once
#include "UIElement.h"
#include "GameData.h"

class CardUI : public UIElement {
private:
    CardData* data; // 바인딩된 카드 데이터
    int baseY;

public:
    CardUI(int x, int y, CardData* cardData);

    void SetBasePosition(int newX, int newY);
    int GetX() const { return x; }
    int GetY() const { return y; }
    int GetWidth() const { return width; }

    bool Update(InputManager& input) override;
    void Render(ScreenManager& screen) override;
};