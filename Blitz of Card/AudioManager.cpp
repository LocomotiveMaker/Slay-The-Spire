// -----------------------------------------------------------------------------
// @file       AudioManager.cpp
// @brief      BGM/SFX 재생 및 볼륨 제어 구현부
// -----------------------------------------------------------------------------
#include "AudioManager.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <vector>

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

std::wstring AudioManager::ResolveAudioPath(const std::wstring& filename) const {
    if (filename.empty()) {
        return {};
    }

    namespace fs = std::filesystem;
    const fs::path candidatePath(filename);
    if (candidatePath.is_absolute() && fs::exists(candidatePath)) {
        return candidatePath.wstring();
    }

    const std::vector<fs::path> candidates = {
        fs::path(exeDir) / filename,
        fs::path(exeDir) / L"Assets" / L"Audio" / filename,
        fs::path(exeDir) / L"Audio" / filename,
        fs::path(exeDir) / L".." / L".." / filename,
        fs::path(exeDir) / L".." / L".." / L"Assets" / L"Audio" / filename,
        fs::path(exeDir) / L".." / L".." / L"Blitz of Card" / L"Assets" / L"Audio" / filename
    };

    for (const fs::path& candidate : candidates) {
        std::error_code ec;
        const fs::path normalized = fs::weakly_canonical(candidate, ec);
        const fs::path finalPath = ec ? candidate : normalized;
        if (fs::exists(finalPath)) {
            return finalPath.wstring();
        }
    }

    return {};
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
    const std::wstring fullPath = ResolveAudioPath(filename);
    if (fullPath.empty()) {
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
    PlayEffectWithSpeed(filename, alias, 1000, loop);
}

void AudioManager::PlayEffectWithSpeed(const std::wstring& filename, const std::wstring& alias, int speedPermille, bool loop) {
    const std::wstring fullPath = ResolveAudioPath(filename);
    if (fullPath.empty()) {
        return;
    }

    mciSendStringW((L"stop " + alias).c_str(), nullptr, 0, nullptr);
    mciSendStringW((L"close " + alias).c_str(), nullptr, 0, nullptr);

    const std::wstring openCommand = L"open \"" + fullPath + L"\" type mpegvideo alias " + alias;
    mciSendStringW(openCommand.c_str(), nullptr, 0, nullptr);

    const int safeSpeed = (std::max)(500, (std::min)(2000, speedPermille));
    const std::wstring speedCommand = L"set " + alias + L" speed " + std::to_wstring(safeSpeed);
    mciSendStringW(speedCommand.c_str(), nullptr, 0, nullptr);

    const std::wstring playCommand = L"play " + alias + (loop ? L" repeat" : L" from 0");
    mciSendStringW(playCommand.c_str(), nullptr, 0, nullptr);

    const std::wstring volumeCommand = L"setaudio " + alias + L" volume to " + std::to_wstring(BuildScaledVolumeValue(100.0f, false));
    mciSendStringW(volumeCommand.c_str(), nullptr, 0, nullptr);
}

void AudioManager::StopEffect(const std::wstring& alias) {
    mciSendStringW((L"stop " + alias).c_str(), nullptr, 0, nullptr);
    mciSendStringW((L"close " + alias).c_str(), nullptr, 0, nullptr);
}

void AudioManager::Shutdown() {
    queuedBgmFile.clear();
    currentBgmFile.clear();
    isFadingOutForSwitch = false;
    fadeDurationSec = 0.0f;
    fadeElapsedSec = 0.0f;

    // 종료 시점에 MCI alias가 남아 있으면 일부 환경에서 종료가 거칠게 끝날 수 있다.
    mciSendStringW(L"stop all", nullptr, 0, nullptr);
    mciSendStringW(L"close all", nullptr, 0, nullptr);
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
