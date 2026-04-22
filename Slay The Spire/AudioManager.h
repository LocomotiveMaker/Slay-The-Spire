// -----------------------------------------------------------------------------
// @file       AudioManager.h
// @brief      BGM, SFX 및 공간 음향(Spatial Audio) 관리 클래스
// -----------------------------------------------------------------------------
#pragma once
#include <string>
#include <windows.h>

class AudioManager {
private:
    int masterVolume;
    int bgmVolume;
    int sfxVolume;
    std::wstring exeDir;
    std::wstring currentBgmFile;
    std::wstring queuedBgmFile;
    float currentBgmPercent;
    float fadeStartPercent;
    float targetBgmPercent;
    float fadeDurationSec;
    float fadeElapsedSec;
    float queuedFadeInSec;
    float queuedTargetPercent;
    bool isFadingOutForSwitch;

    std::wstring GetExeDir() const;
    int BuildScaledVolumeValue(float logicalPercent, bool bgmChannel) const;
    void SetBGMVolumeInternal(float logicalPercent);

public:
    AudioManager();

    void PlayBGM(const std::wstring& filename, float logicalPercent = 100.0f);
    void QueueBGMFade(const std::wstring& filename, float targetPercent, float fadeOutSec, float fadeInSec);
    void FadeCurrentBGMTo(float targetPercent, float durationSec);
    void Update(float deltaTimeSec);

    void SetVolumes(int master, int bgm, int sfx);
    void PlayEffect(const std::wstring& filename, const std::wstring& alias, bool loop = false);
    void StopEffect(const std::wstring& alias);

    // 두 점 사이의 거리를 계산하여 지정된 alias 채널의 볼륨을 실시간으로 조절
    void UpdateSpatialVolume(int sourceX, int sourceY, int listenerX, int listenerY, const std::wstring& alias, int maxDistance);
};
