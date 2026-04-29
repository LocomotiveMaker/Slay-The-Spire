// -----------------------------------------------------------------------------
// @file       SaveManager.h
// @brief      이어하기, 기록, 설정 저장/불러오기 인터페이스
// -----------------------------------------------------------------------------
#pragma once
#include "RunState.h"
#include <filesystem>
#include <vector>

class SaveManager {
public:
    static std::filesystem::path GetProjectRoot();
    static std::filesystem::path GetSaveDirectory();

    static SettingsData LoadSettings();
    static bool SaveSettings(const SettingsData& settings);

    static bool HasContinueRun();
    static bool LoadContinueRun(RunStateData& run);
    static bool SaveContinueRun(const RunStateData& run);
    static bool DeleteContinueRun();

    static GlobalStatsData LoadGlobalStats();
    static bool SaveGlobalStats(const GlobalStatsData& stats);

    static std::vector<RunRecordData> LoadRunRecords();
    static bool SaveRunRecords(const std::vector<RunRecordData>& records);
    static bool AppendRunRecord(const RunRecordData& record, GlobalStatsData& stats);
};
