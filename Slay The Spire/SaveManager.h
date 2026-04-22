// -----------------------------------------------------------------------------
// @file       SaveManager.h
// @brief      Save/load helpers for continue data, records, and settings.
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
