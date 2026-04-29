// -----------------------------------------------------------------------------
// @file       AudioManager.cpp
// @brief      BGM/SFX 재생 및 볼륨 제어 구현부
// -----------------------------------------------------------------------------
#include "AudioManager.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

#pragma comment(lib, "winmm.lib")

AudioManager::AudioManager()
    : masterVolume(100),
    bgmVolume(100),
    sfxVolume(100),
    currentBgmPercent(0.0f),
    fadeStartPercent(0.0f),
    targetBgmPercent(0.0f),
    fadeDurationSec(0.0f),
    fadeElapsedSec(0.0f),
    queuedFadeInSec(0.0f),
    queuedTargetPercent(0.0f),
    isFadingOutForSwitch(false) {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring dir(path);
    exeDir = dir.substr(0, dir.find_last_of(L"\\/"));
}

std::wstring AudioManager::GetExeDir() const {
    return exeDir;
}

int AudioManager::BuildScaledVolumeValue(float logicalPercent, bool bgmChannel) const {
    const float safePercent = (std::max)(0.0f, (std::min)(100.0f, logicalPercent));
    const float channelVolume = bgmChannel ? static_cast<float>(bgmVolume) : static_cast<float>(sfxVolume);
    const float normalized = (safePercent / 100.0f) * (static_cast<float>(masterVolume) / 100.0f) * (channelVolume / 100.0f);
    return static_cast<int>(std::round(normalized * 1000.0f));
}

void AudioManager::SetBGMVolumeInternal(float logicalPercent) {
    currentBgmPercent = (std::max)(0.0f, (std::min)(100.0f, logicalPercent));
    const std::wstring volumeCommand = L"setaudio bgm volume to " + std::to_wstring(BuildScaledVolumeValue(currentBgmPercent, true));
    mciSendStringW(volumeCommand.c_str(), nullptr, 0, nullptr);
}

void AudioManager::PlayBGM(const std::wstring& filename, float logicalPercent) {
    const std::wstring fullPath = exeDir + L"\\" + filename;
    if (!std::filesystem::exists(fullPath)) {
        return;
    }

    mciSendStringW(L"stop bgm", nullptr, 0, nullptr);
    mciSendStringW(L"close bgm", nullptr, 0, nullptr);

    const std::wstring openCommand = L"open \"" + fullPath + L"\" type mpegvideo alias bgm";
    mciSendStringW(openCommand.c_str(), nullptr, 0, nullptr);
    mciSendStringW(L"play bgm repeat", nullptr, 0, nullptr);

    currentBgmFile = filename;
    queuedBgmFile.clear();
    isFadingOutForSwitch = false;
    fadeElapsedSec = 0.0f;
    fadeDurationSec = 0.0f;
    fadeStartPercent = logicalPercent;
    targetBgmPercent = logicalPercent;
    SetBGMVolumeInternal(logicalPercent);
}

void AudioManager::QueueBGMFade(const std::wstring& filename, float targetPercent, float fadeOutSec, float fadeInSec) {
    if (currentBgmFile.empty()) {
        PlayBGM(filename, 0.0f);
        FadeCurrentBGMTo(targetPercent, fadeInSec);
        return;
    }

    if (currentBgmFile == filename) {
        FadeCurrentBGMTo(targetPercent, fadeInSec);
        return;
    }

    queuedBgmFile = filename;
    queuedFadeInSec = fadeInSec;
    queuedTargetPercent = targetPercent;
    isFadingOutForSwitch = true;
    FadeCurrentBGMTo(0.0f, fadeOutSec);
}

void AudioManager::FadeCurrentBGMTo(float targetPercent, float durationSec) {
    fadeStartPercent = currentBgmPercent;
    targetBgmPercent = (std::max)(0.0f, (std::min)(100.0f, targetPercent));
    fadeDurationSec = (std::max)(0.001f, durationSec);
    fadeElapsedSec = 0.0f;
}

void AudioManager::Update(float deltaTimeSec) {
    if (fadeDurationSec <= 0.0f) {
        return;
    }

    fadeElapsedSec += deltaTimeSec;
    const float t = (std::min)(1.0f, fadeElapsedSec / fadeDurationSec);
    const float lerpedPercent = fadeStartPercent + ((targetBgmPercent - fadeStartPercent) * t);
    SetBGMVolumeInternal(lerpedPercent);

    if (t < 1.0f) {
        return;
    }

    fadeDurationSec = 0.0f;
    fadeElapsedSec = 0.0f;
    fadeStartPercent = currentBgmPercent;

    if (isFadingOutForSwitch && currentBgmPercent <= 0.0f && !queuedBgmFile.empty()) {
        const std::wstring queuedTrack = queuedBgmFile;
        queuedBgmFile.clear();
        isFadingOutForSwitch = false;
        PlayBGM(queuedTrack, 0.0f);
        FadeCurrentBGMTo(queuedTargetPercent, queuedFadeInSec);
    }
}

void AudioManager::SetVolumes(int master, int bgm, int sfx) {
    masterVolume = (std::max)(0, (std::min)(100, master));
    bgmVolume = (std::max)(0, (std::min)(100, bgm));
    sfxVolume = (std::max)(0, (std::min)(100, sfx));
    SetBGMVolumeInternal(currentBgmPercent);
}

void AudioManager::PlayEffect(const std::wstring& filename, const std::wstring& alias, bool loop) {
    const std::wstring fullPath = exeDir + L"\\" + filename;
    if (!std::filesystem::exists(fullPath)) {
        return;
    }

    mciSendStringW((L"stop " + alias).c_str(), nullptr, 0, nullptr);
    mciSendStringW((L"close " + alias).c_str(), nullptr, 0, nullptr);

    const std::wstring openCommand = L"open \"" + fullPath + L"\" type mpegvideo alias " + alias;
    mciSendStringW(openCommand.c_str(), nullptr, 0, nullptr);

    const std::wstring playCommand = L"play " + alias + (loop ? L" repeat" : L" from 0");
    mciSendStringW(playCommand.c_str(), nullptr, 0, nullptr);

    const std::wstring volumeCommand = L"setaudio " + alias + L" volume to " + std::to_wstring(BuildScaledVolumeValue(100.0f, false));
    mciSendStringW(volumeCommand.c_str(), nullptr, 0, nullptr);
}

void AudioManager::StopEffect(const std::wstring& alias) {
    mciSendStringW((L"stop " + alias).c_str(), nullptr, 0, nullptr);
    mciSendStringW((L"close " + alias).c_str(), nullptr, 0, nullptr);
}

void AudioManager::UpdateSpatialVolume(int sourceX, int sourceY, int listenerX, int listenerY, const std::wstring& alias, int maxDistance) {
    const double dx = static_cast<double>(listenerX - sourceX);
    const double dy = static_cast<double>(listenerY - sourceY) * 2.0;
    const double distance = std::sqrt((dx * dx) + (dy * dy));

    double normalizedDistance = (maxDistance > 0) ? (distance / maxDistance) : 1.0;
    if (normalizedDistance > 1.0) normalizedDistance = 1.0;

    const double volumeRatio = std::pow(1.0 - normalizedDistance, 2.0);
    const float logicalPercent = static_cast<float>(volumeRatio * 100.0);
    const std::wstring volumeCommand = L"setaudio " + alias + L" volume to " + std::to_wstring(BuildScaledVolumeValue(logicalPercent, false));
    mciSendStringW(volumeCommand.c_str(), nullptr, 0, nullptr);
}
