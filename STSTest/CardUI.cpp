#include "CardUI.h"

CardUI::CardUI(int x, int y, CardData* cardData)
    : UIElement(x, y, 28, 18), data(cardData), baseY(y) {
}

void CardUI::SetBasePosition(int newX, int newY) {
    SetPosition(newX, newY);
    baseY = newY;
}

bool CardUI::Update(InputManager& input) {
    bool hit = UIElement::Update(input);

    // 호버 상태 시 기준 위치(baseY)에서 위로 3칸 이동
    y = isHovered ? baseY - 3 : baseY;

    if (isHovered && input.IsLeftClickDown()) {
        // 내부 상호작용 플래그 처리 필요 시 작성
    }
    return hit;
}

void CardUI::Render(ScreenManager& screen) {
    if (data == nullptr) return;

    WORD color = isHovered ? COLOR_YELLOW : COLOR_WHITE;

    screen.DrawString(x, y + 0, ",--------------------------.", color);
    screen.DrawString(x, y + 1, "|[" + std::to_string(data->cost) + "]                       |", color);
    screen.DrawString(x, y + 2, "|                          |", color);

    int namePad = (26 - (int)data->name.length()) / 2;
    if (namePad < 0) namePad = 0;
    std::string nameLine = "|" + std::string(namePad, ' ') + data->name + std::string(26 - namePad - data->name.length(), ' ') + "|";

    screen.DrawString(x, y + 3, nameLine, color);
    screen.DrawString(x, y + 4, "|                          |", color);
    screen.DrawString(x, y + 5, "|       //========\\\\       |", color);
    screen.DrawString(x, y + 6, "|       ||  ART   ||       |", color);
    screen.DrawString(x, y + 7, "|       \\\\========//       |", color);
    screen.DrawString(x, y + 8, "|                          |", color);

    // 임시 하드코딩된 설명을 데이터 기반으로 변경 (단순 출력)
    // 실제 구현 시 워드랩(Word Wrap) 로직이 추가되어야 함
    int descriptionPad = (26 - (int)data->description.length()) / 2;
    std::string descriptionLine = "|" + std::string(descriptionPad, ' ') + data->description + std::string(26 - descriptionPad - data->description.length(), ' ') + "|";
    screen.DrawString(x, y + 9, descriptionLine, color);

    screen.DrawString(x, y + 10, "|                          |", color);
    screen.DrawString(x, y + 11, "|                          |", color);
    screen.DrawString(x, y + 12, "|                          |", color);
    screen.DrawString(x, y + 13, "|                          |", color);
    screen.DrawString(x, y + 14, "|                          |", color);

    std::string typeStr = (data->type == CardType::Attack) ? "Attack" : "Skill";
    int typePad = (26 - (int)typeStr.length()) / 2;
    if (typePad < 0) typePad = 0;
    std::string typeLine = "|" + std::string(typePad, ' ') + typeStr + std::string(26 - typePad - typeStr.length(), ' ') + "|";

    screen.DrawString(x, y + 15, typeLine, color);
    screen.DrawString(x, y + 16, "|                          |", color);
    screen.DrawString(x, y + 17, "`--------------------------'", color);
}