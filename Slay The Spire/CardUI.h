#pragma once
#include "GameData.h"
#include "UIElement.h"
#include <string>
#include <vector>

class CardUI : public UIElement {
private:
    CardData* data;
    int baseY;
    int rightOcclusionChars;
    WORD frameColor;
    bool playable;
    std::wstring cachedNameLine;
    std::wstring cachedTypeLine;
    std::vector<std::wstring> cachedDescriptionLines;

    void RebuildLayoutCache();

public:
    CardUI(int x, int y, CardData* cardData);

    void SetBasePosition(int newX, int newY);
    void SetRightOcclusion(int chars);
    void SetFrameColor(WORD color);
    void SetPlayable(bool canPlay);
    int GetX() const { return x; }
    int GetY() const { return y; }
    int GetWidth() const { return width; }

    bool Update(InputManager& input) override;
    void Render(ScreenManager& screen) override;
};
