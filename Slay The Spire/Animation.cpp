// -----------------------------------------------------------------------------
// @file       Animation.cpp
// @brief      프레임 애니메이션 재생 로직 구현부
// -----------------------------------------------------------------------------
#include "Animation.h"

Animator::Animator(int startX, int startY, const std::vector<std::vector<std::string>>& animFrames, int delayMs, AnimMode playMode, WORD colorCode)
    : x(startX), y(startY), frames(animFrames), frameDelayMs(delayMs), mode(playMode), color(colorCode), currentFrame(0), isPlaying(true)
{
    lastUpdateTime = std::chrono::steady_clock::now();
}

void Animator::Play() {
    isPlaying = true;
    currentFrame = 0;
    lastUpdateTime = std::chrono::steady_clock::now();
}

void Animator::Stop() {
    isPlaying = false;
}

void Animator::SetPosition(int newX, int newY) {
    x = newX;
    y = newY;
}

bool Animator::IsFinished() const {
    if (frames.empty()) return true;
    return (!isPlaying && mode == AnimMode::ONE_SHOT);
}

void Animator::Update() {
    if (!isPlaying || frames.empty()) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdateTime).count();

    // 지정된 딜레이 시간이 경과하면 다음 프레임으로 전환
    if (elapsed >= frameDelayMs) {
        currentFrame++;
        lastUpdateTime = now;

        // 마지막 프레임 도달 시 모드에 따른 처리
        if (currentFrame >= frames.size()) {
            if (mode == AnimMode::LOOP) {
                currentFrame = 0;
            }
            else {
                currentFrame = frames.size() - 1; // 마지막 프레임에 고정
                isPlaying = false; // 재생 종료
            }
        }
    }
}

void Animator::Render(ScreenManager& screen) {
    if (frames.empty() || (mode == AnimMode::ONE_SHOT && !isPlaying && currentFrame == frames.size() - 1)) {
        // One-shot 애니메이션이 끝난 후에는 화면에서 숨김 처리
        // (필요 시 마지막 프레임을 남기려면 로직 수정 가능)
        return;
    }

    const auto& currentArt = frames[currentFrame];
    for (size_t i = 0; i < currentArt.size(); ++i) {
        screen.DrawString(x, y + static_cast<int>(i), currentArt[i], color);
    }
}
