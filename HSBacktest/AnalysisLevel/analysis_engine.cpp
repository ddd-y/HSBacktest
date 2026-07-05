#include "analysis_engine.h"
#include "../MyLog/Logger.h"
#include "../Datalevel/Global_data.h"
#include "../Datalevel/factor_calculate/factor_registry.h"
#include "../EngineLevel/BacktestEngine.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <filesystem>

// ==========================================
// 排序工具
// ==========================================
static std::vector<size_t> rank_by_sharpe(const std::vector<BacktestSummary>& all_summaries) {
    std::vector<size_t> r(all_summaries.size());
    std::iota(r.begin(), r.end(), 0);
    std::sort(r.begin(), r.end(),
        [&](size_t a, size_t b) {
            return all_summaries[a].sharpe_ratio > all_summaries[b].sharpe_ratio;
        });
    return r;
}

// ==========================================
// RunAll（排一次序，两个分析共享）
// ==========================================
void AnalysisEngine::RunAll(
    const std::vector<BacktestSummary>& all_summaries,
    double init_capital,
    const std::string& report_path,
    const std::string& csv_dir)
{
    if (all_summaries.empty()) return;
    auto rank = rank_by_sharpe(all_summaries);
    RunRobustnessImpl(all_summaries, rank, 20, report_path);
    ExportTopResultsImpl(all_summaries, rank, 10, init_capital, csv_dir);
}

// ==========================================
// 公开接口（内部排序后委托）
// ==========================================
void AnalysisEngine::RunRobustness(
    const std::vector<BacktestSummary>& all_summaries,
    int top_n,
    const std::string& report_path)
{
    if (all_summaries.empty()) return;
    auto rank = rank_by_sharpe(all_summaries);
    RunRobustnessImpl(all_summaries, rank, top_n, report_path);
}

void AnalysisEngine::ExportTopResults(
    const std::vector<BacktestSummary>& all_summaries,
    int top_n,
    double init_capital,
    const std::string& output_dir)
{
    if (all_summaries.empty()) return;
    auto rank = rank_by_sharpe(all_summaries);
    ExportTopResultsImpl(all_summaries, rank, top_n, init_capital, output_dir);
}

// ==========================================
// RunRobustness 实现
// ==========================================
void AnalysisEngine::RunRobustnessImpl(
    const std::vector<BacktestSummary>& all_summaries,
    const std::vector<size_t>& rank,
    int top_n,
    const std::string& report_path)
{
    GlobalData* gd = GlobalData::GetGlobalData();
    if (!gd) {
        LOG_ERROR("AnalysisEngine::RunRobustness - GlobalData not initialized");
        return;
    }

    std::ofstream ofs(report_path);
    if (!ofs.is_open()) {
        LOG_ERROR("AnalysisEngine::RunRobustness - cannot open {}", report_path);
        return;
    }

    auto out = [&](const std::string& line) { ofs << line << "\n"; };

    int N = std::min(top_n, static_cast<int>(all_summaries.size()));
    const auto& reg = GetFactorRegistry();

    // 复制 Top-N 到本地
    std::vector<std::array<double, FACTOR_NUM>> top_w(N);
    std::vector<int> top_tn(N);
    std::vector<double> top_sharpe(N), top_maxdd(N), top_annual(N), top_vol(N), top_turnover(N), top_totalret(N);

    for (int i = 0; i < N; ++i) {
        size_t si = rank[i];
        int pi = all_summaries[si].param_index;
        const auto& w = gd->GetWeights(pi);
        std::copy(w.begin(), w.end(), top_w[i].begin());
        top_tn[i]      = gd->GetTopN(pi);
        top_sharpe[i]  = all_summaries[si].sharpe_ratio;
        top_maxdd[i]   = all_summaries[si].max_drawdown;
        top_annual[i]  = all_summaries[si].annual_return;
        top_vol[i]     = all_summaries[si].annual_volatility;
        top_turnover[i]= all_summaries[si].avg_turnover;
        top_totalret[i]= all_summaries[si].total_return;
    }

    out("========================================");
    out("  HSBacktest 策略鲁棒性分析");
    out("========================================");

    // ---- Top-10 展示 ----
    int show_n = std::min(10, N);
    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "  ── Top-%d 最优参数 (按 sharpe) ──", show_n);
        out(buf);
    }
    {
        std::string h = " Rank sharpe  max_dd  annual  vol    turnover  total_ret ";
        for (int f = 0; f < FACTOR_NUM; ++f) {
            char buf[12];
            std::snprintf(buf, sizeof(buf), "%-7s", reg[f].name);
            h += buf;
        }
        h += "top_n";
        out(h);
    }
    for (int i = 0; i < show_n; ++i) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            " %4d %+.4f %5.1f%% %+6.1f%% %5.1f%% %6.1f%%   %+6.1f%%   ",
            i + 1, top_sharpe[i], top_maxdd[i] * 100.0, top_annual[i] * 100.0,
            top_vol[i] * 100.0, top_turnover[i] * 100.0, top_totalret[i] * 100.0);
        std::string line = buf;
        for (int f = 0; f < FACTOR_NUM; ++f) {
            std::snprintf(buf, sizeof(buf), "%-7.3f", top_w[i][f]);
            line += buf;
        }
        std::snprintf(buf, sizeof(buf), "%d", top_tn[i]);
        line += buf;
        out(line);
    }

    // ---- 权重分布 ----
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "  ── Top-%d 权重分布 ──", N);
        out(buf);
    }
    out("  因子             均值    标准差    最小    最大");
    for (int f = 0; f < FACTOR_NUM; ++f) {
        double mean = 0.0, sq = 0.0;
        double vmin = top_w[0][f], vmax = top_w[0][f];
        for (int i = 0; i < N; ++i) {
            double v = top_w[i][f];
            mean += v;
            if (v < vmin) vmin = v;
            if (v > vmax) vmax = v;
        }
        mean /= N;
        for (int i = 0; i < N; ++i) sq += std::pow(top_w[i][f] - mean, 2);
        double stddev = std::sqrt(sq / N);
        const char* flag = (mean > 0.01 && stddev / mean < 0.3) ? " ✓" : " ~";
        char buf[128];
        std::snprintf(buf, sizeof(buf), "  %-16s %7.4f %7.4f %7.4f %7.4f%s",
            reg[f].name, mean, stddev, vmin, vmax, flag);
        out(buf);
    }

    // ---- 退化分析 ----
    {
        double best = top_sharpe[0];
        double threshold = best * 0.9;
        int count = 0;
        for (size_t i = 0; i < all_summaries.size(); ++i)
            if (all_summaries[i].sharpe_ratio >= threshold) count++;

        out("  ── 退化分析 ──");
        char buf[128];
        std::snprintf(buf, sizeof(buf), "  最优 sharpe:   %.4f", best); out(buf);
        std::snprintf(buf, sizeof(buf), "  90%% 阈值:      %.4f", threshold); out(buf);
        std::snprintf(buf, sizeof(buf), "  达标参数组:    %d/%d  (%.1f%%)",
            count, (int)all_summaries.size(), 100.0 * count / all_summaries.size());
        out(buf);
        if (count > static_cast<int>(all_summaries.size()) * 15 / 100)
            out("  → 平坦高原：策略对参数不敏感，鲁棒性较好");
        else if (count < static_cast<int>(all_summaries.size()) * 3 / 100)
            out("  → 尖锐峰值：可能过拟合，实盘谨慎");
        else
            out("  → 中等敏感度");
    }

    // ---- 概要 ----
    {
        double best = top_sharpe[0];
        double worst = all_summaries[rank.back()].sharpe_ratio;
        double median = all_summaries[rank[all_summaries.size() / 2]].sharpe_ratio;
        out("  ── 概要 ──");
        char buf[128];
        std::snprintf(buf, sizeof(buf), "  sharpe 范围:  %.4f ~ %.4f  (中位数 %.4f)", worst, best, median);
        out(buf);
        std::snprintf(buf, sizeof(buf), "  总参数组合:   %d", (int)all_summaries.size());
        out(buf);
    }

    out("========================================");
    ofs.close();
    LOG_INFO("分析报告已保存到: {}", report_path);
}

// ==========================================
// ExportTopResults 实现
// ==========================================
void AnalysisEngine::ExportTopResultsImpl(
    const std::vector<BacktestSummary>& all_summaries,
    const std::vector<size_t>& rank,
    int top_n,
    double init_capital,
    const std::string& output_dir)
{
    GlobalData* gd = GlobalData::GetGlobalData();
    if (!gd) {
        LOG_ERROR("AnalysisEngine::ExportTopResults - GlobalData not initialized");
        return;
    }

    int N = std::min(top_n, static_cast<int>(all_summaries.size()));
    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);

    LOG_INFO("导出 Top-{} 回测 CSV 到 {} ...", N, output_dir);

    for (int i = 0; i < N; ++i) {
        size_t si = rank[i];
        int pi = all_summaries[si].param_index;
        auto w = gd->GetWeights(pi);
        int tn = gd->GetTopN(pi);

        AdjustParam ap;
        ap.factor_weights = w;
        ap.top_n = tn;
        gd->SetCustomAdjustParams({ ap });

        BacktestEngine engine;
        engine.Initialize(init_capital, 0);
        engine.Run();

        char nav_file[256], trade_file[256];
        std::snprintf(nav_file, sizeof(nav_file), "%s/rank_%02d_nav.csv", output_dir.c_str(), i + 1);
        std::snprintf(trade_file, sizeof(trade_file), "%s/rank_%02d_trades.csv", output_dir.c_str(), i + 1);
        engine.ExportNavToCsv(nav_file);
        engine.ExportTradesToCsv(trade_file);

        const auto& s = all_summaries[si];
        LOG_INFO("  rank {}: sharpe={:+.4f}  max_dd={:.1f}%  annual={:+.1f}%  top_n={}",
            i + 1, s.sharpe_ratio, s.max_drawdown * 100.0, s.annual_return * 100.0, tn);
    }

    LOG_INFO("导出完成: {} 组 → {}/", N, output_dir);
}
