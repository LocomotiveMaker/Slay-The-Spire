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
    int sfxVolume;
    std::wstring exeDir;

    std::wstring GetExeDir() const;

public:
    AudioManager();

    void PlayBGM(const std::wstring& filename);

    void SetVolumes(int master, int sfx);
    void PlayEffect(const std::wstring& filename, const std::wstring& alias, bool loop = false);
    void StopEffect(const std::wstring& alias);

    // 두 점 사이의 거리를 계산하여 지정된 alias 채널의 볼륨을 실시간으로 조절
    void UpdateSpatialVolume(int sourceX, int sourceY, int listenerX, int listenerY, const std::wstring& alias, int maxDistance);
};