#pragma once
#include <vector>
#include <string>
#include "../CollectorLevel/BacktestSummary.h"

constexpr const char* CSV_DIR = "analysis_results/";
constexpr const char* REPORT_PATH = "analysis_report.txt";

// ==========================================
// AnalysisEngine — 回测后统计分析
// ==========================================
class AnalysisEngine {
public:
    // 一键运行：鲁棒性报告 + 导出 Top-10 CSV（只排一次序）
    static void RunAll(
        const std::vector<BacktestSummary>& all_summaries,
        double init_capital = 1000000.0,
        const std::string& report_path = REPORT_PATH,
        const std::string& csv_dir = CSV_DIR);

    // 单独跑鲁棒性报告（内部会排序）
    static void RunRobustness(
        const std::vector<BacktestSummary>& all_summaries,
        int top_n = 20,
        const std::string& report_path = REPORT_PATH);

    // 单独导出 Top-N CSV（内部会排序）
    static void ExportTopResults(
        const std::vector<BacktestSummary>& all_summaries,
        int top_n = 10,
        double init_capital = 1000000.0,
        const std::string& output_dir = CSV_DIR);

private:
    // 共享实现：传入已排序的 rank
    static void RunRobustnessImpl(
        const std::vector<BacktestSummary>& all_summaries,
        const std::vector<size_t>& rank,
        int top_n,
        const std::string& report_path);

    static void ExportTopResultsImpl(
        const std::vector<BacktestSummary>& all_summaries,
        const std::vector<size_t>& rank,
        int top_n,
        double init_capital,
        const std::string& output_dir);
};
