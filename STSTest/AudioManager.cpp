// -----------------------------------------------------------------------------
// @file       AudioManager.cpp
// -----------------------------------------------------------------------------
#include "AudioManager.h"
#include <cmath>

#pragma comment(lib, "winmm.lib")

AudioManager::AudioManager() : masterVolume(5), sfxVolume(5) {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring dir(path);
    exeDir = dir.substr(0, dir.find_last_of(L"\\/"));
}

std::wstring AudioManager::GetExeDir() const {
    return exeDir;
}

void AudioManager::PlayBGM(const std::wstring& filename) {
    std::wstring fullPath = exeDir + L"\\" + filename;

    // 기존 BGM 종료
    mciSendStringW(L"stop bgm", NULL, 0, NULL);
    mciSendStringW(L"close bgm", NULL, 0, NULL);

    // BGM 채널 열기 및 반복 재생
    std::wstring openCmd = L"open \"" + fullPath + L"\" type mpegvideo alias bgm";
    mciSendStringW(openCmd.c_str(), NULL, 0, NULL);
    mciSendStringW(L"play bgm repeat", NULL, 0, NULL);
}

void AudioManager::SetVolumes(int master, int sfx) {
    masterVolume = master;
    sfxVolume = sfx;
}

void AudioManager::PlayEffect(const std::wstring& filename, const std::wstring& alias, bool loop) {
    std::wstring fullPath = exeDir + L"\\" + filename;

    mciSendStringW((L"stop " + alias).c_str(), NULL, 0, NULL);
    mciSendStringW((L"close " + alias).c_str(), NULL, 0, NULL);

    std::wstring openCmd = L"open \"" + fullPath + L"\" type mpegvideo alias " + alias;
    mciSendStringW(openCmd.c_str(), NULL, 0, NULL);

    std::wstring playCmd = L"play " + alias + (loop ? L" repeat" : L" from 0");
    mciSendStringW(playCmd.c_str(), NULL, 0, NULL);
}

void AudioManager::StopEffect(const std::wstring& alias) {
    mciSendStringW((L"stop " + alias).c_str(), NULL, 0, NULL);
    mciSendStringW((L"close " + alias).c_str(), NULL, 0, NULL);
}

void AudioManager::UpdateSpatialVolume(int sourceX, int sourceY, int listenerX, int listenerY, const std::wstring& alias, int maxDistance) {
    // 콘솔 종횡비 보정
    double dx = static_cast<double>(listenerX - sourceX);
    double dy = static_cast<double>(listenerY - sourceY) * 2.0;
    double distance = std::sqrt((dx * dx) + (dy * dy));

    // 거리를 0.0 ~ 1.0 사이 값으로 정규화
    double normalizedDistance = (maxDistance > 0) ? (distance / maxDistance) : 1.0;
    if (normalizedDistance > 1.0) normalizedDistance = 1.0;

    // 비선형 볼륨 곡선 적용
    // (1.0 - 정규화된 거리) 값의 제곱을 사용하여, 가까울수록 볼륨이 급격히 증가하도록 함.
    // 지수(2.0)를 3.0 등으로 높이면 곡선이 더 가파르게 변함.
    double volumeRatio = std::pow(1.0 - normalizedDistance, 2.0);

    int baseVol = 1000; // 최대 볼륨은 1000으로 고정
    int dynamicVolume = static_cast<int>(baseVol * volumeRatio);

    if (dynamicVolume < 0) dynamicVolume = 0;

    std::wstring volCmd = L"setaudio " + alias + L" volume to " + std::to_wstring(dynamicVolume);
    mciSendStringW(volCmd.c_str(), NULL, 0, NULL);
}