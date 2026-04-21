// -----------------------------------------------------------------------------
// @file       Animation.h
// @brief      프레임 기반 아스키 아트 애니메이션 렌더링 클래스
// -----------------------------------------------------------------------------
#pragma once
#include "ScreenManager.h"
#include <vector>
#include <string>
#include <chrono>

// 애니메이션 재생 모드
enum class AnimMode {
    LOOP,      // 무한 반복 (모닥불, 버프 오라 등)
    ONE_SHOT   // 1회 재생 후 정지 (타격 이펙트, 번개 등)
};

class Animator {
private:
    int x, y;
    std::vector<std::vector<std::string>> frames; // 3차원 배열 구조 (프레임 묶음)
    int frameDelayMs; // 프레임 간 딜레이 (재생 속도)
    AnimMode mode;
    WORD color;

    int currentFrame;
    bool isPlaying;
    std::chrono::steady_clock::time_point lastUpdateTime;

public:
    Animator(int startX, int startY, const std::vector<std::vector<std::string>>& animFrames, int delayMs, AnimMode playMode, WORD colorCode);

    void Play();
    void Stop();
    void SetPosition(int newX, int newY);

    // 1회성 애니메이션이 재생을 완료했는지 확인 (로직 동기화용)
    bool IsFinished() const;

    void Update();
    void Render(ScreenManager& screen);
};