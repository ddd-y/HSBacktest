#pragma once

#include "../HSBacktest.h"
#include "../MyLog/Logger.h"
#include "../Datalevel/read_csvdata/read_csv.h"
#include "../Datalevel/Global_data.h"
#include "../Datalevel/param_builder/param_builder.h"
#include "../ConfigLvevl/configer.h"
#include "../EngineLevel/BacktestEngine.h"
#include "../CollectorLevel/performance_collector.h"
#include "../Datalevel/stock_k_data.h"
#include "../Datalevel/factor_calculate/factorbase.h"
#include <fstream>
#include <iomanip>
#include <cmath>
#include <cstdio>
#include <string>

// 断言宏
#define TEST_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            LOG_ERROR("[FAIL] {} ({}:{})", msg, __FILE__, __LINE__); \
            passed = false; \
        } else { \
            LOG_INFO("[PASS] {}", msg); \
        } \
    } while(0)

static bool passed = true;

// 生成单只股票22个交易日的测试CSV文件
void generate_fulltest_csv() {
    {
        std::ofstream ofs("fulltest_daily.csv");
        ofs << "trade_date,close,open,adj_factor,industry_code,is_suspended,is_delisted,is_limit_up,is_limit_down" << std::endl;
        double price = 100.0;
        for (int i = 0; i < 22; ++i) {
            ofs << (20250101 + i) << ","
                << std::fixed << std::setprecision(2) << price << ","
                << (price - 0.5) << ",1.0," << ((i % 5 == 0) ? 1 : 2) << ",0,0,0,0" << std::endl;
            price += (i % 5 == 0 ? -2.0 : 1.0);
        }
    }
    {
        std::ofstream ofs("fulltest_daily_extended.csv");
        ofs << "trade_date,high,low,volume,amount" << std::endl;
        double price = 100.0;
        double vol = 1000000.0;
        for (int i = 0; i < 22; ++i) {
            ofs << (20250101 + i) << ","
                << std::fixed << std::setprecision(2) << (price + 0.5) << ","
                << (price - 0.5) << "," << vol << "," << (vol * price) << std::endl;
            price += (i % 5 == 0 ? -2.0 : 1.0);
            vol += 50000.0;
        }
    }
    {
        std::ofstream ofs("fulltest_daily_financial.csv");
        ofs << "cash_dividend,split_ratio,total_shares,float_shares,eps_ttm,pe_ttm,pb_lf,roe_ttm" << std::endl;
        for (int i = 0; i < 22; ++i) {
            ofs << "0.0,1.0,100000000,80000000,5.0,20.0,1.5,0.15" << std::endl;
        }
    }
}

void full_pipeline_test()
{
    passed = true;
    HSBacktest::Logger::getInstance().init("fulltest");

    LOG_INFO("========================================");
    LOG_INFO("   全流程检验：数据 → 因子 → 选股 → 交易 → 绩效");
    LOG_INFO("========================================");

    // ==================== 阶段 0：生成数据 ====================
    generate_fulltest_csv();

    // ==================== 阶段 1：数据加载检验 ====================
    LOG_INFO("--- [阶段1] 数据加载 ---");

    auto& search_cfg = Configer::GetParamSearchConfig();
    search_cfg.SetMode(ParamSearchConfiger::SearchMode::RANDOM);
    search_cfg.SetRandomSamples(3);
    search_cfg.SetTopNCandidates({ 20 });
    search_cfg.SetNormalizeWeights(true);

    std::vector<std::string> stock_files = { "fulltest" };
    GlobalData::Init(stock_files);
    GlobalData* gd = GlobalData::GetGlobalData();

    TEST_CHECK(gd != nullptr, "GlobalData 单例不为空");
    TEST_CHECK(gd->get_stock_count() > 0, "股票数 > 0");
    TEST_CHECK(gd->get_stock_count() == 1, "股票数 == 1 (单只测试股票)");
    TEST_CHECK(!gd->get_rebalance_index().empty(), "调仓日列表非空");
    TEST_CHECK(gd->GetAdjustParamCount() > 0, "参数组合数 > 0");

    int stock_count = gd->get_stock_count();
    int param_count = gd->GetAdjustParamCount();

    StockKData* skd = gd->get_stock_k_data(0);
    TEST_CHECK(skd != nullptr, "StockKData 不为空");
    TEST_CHECK(skd->get_daily_datas().size() == 22, "日线数据天数 == 22");

    LOG_INFO("  (股票数={}, 参数组={}, 调仓日={})", stock_count, param_count, gd->get_rebalance_index().size());

    // ==================== 阶段 2：因子计算检验 ====================
    LOG_INFO("--- [阶段2] 因子计算 ---");

    FactorDatabase* fdb = gd->get_factor_database(0);
    TEST_CHECK(fdb != nullptr, "FactorDatabase 不为空");

    if (fdb) {
        int rb_idx = gd->get_rebalance_index()[0];
        double mom  = fdb->get_momentum_20_data().get_momentum_20(rb_idx);
        double vol  = fdb->get_volatility_20_data().get_volatility_20(rb_idx);
        double ep   = fdb->get_ep_ratio_data().get_ep_ratio(rb_idx);
        double mcap = fdb->get_log_mcap_data().get_log_mcap(rb_idx);
        double to   = fdb->get_turnover_20_data().get_turnover_20(rb_idx);

        TEST_CHECK(!std::isnan(mom) && !std::isinf(mom), "动量因子非 NaN/Inf");
        TEST_CHECK(!std::isnan(vol) && !std::isinf(vol), "波动率因子非 NaN/Inf");
        TEST_CHECK(!std::isnan(ep)  && !std::isinf(ep),  "EP因子非 NaN/Inf");
        TEST_CHECK(!std::isnan(mcap)&& !std::isinf(mcap),"对数市值因子非 NaN/Inf");
        TEST_CHECK(!std::isnan(to)  && !std::isinf(to),  "换手率因子非 NaN/Inf");

        LOG_INFO("  (momentum={:.4f}, volatility={:.4f}, ep={:.4f}, mcap={:.4f}, turnover={:.4f})",
            mom, vol, ep, mcap, to);
    }

    // ==================== 阶段 3：参数构建检验 ====================
    LOG_INFO("--- [阶段3] 参数构建 ---");

    TEST_CHECK(param_count == 3, "参数组数 == 3 (3 组随机采样)");

    for (int i = 0; i < param_count; ++i) {
        auto w = gd->GetWeights(i);
        int tn = gd->GetTopN(i);

        double sum = 0.0;
        bool all_finite = true;
        for (int j = 0; j < FACTOR_NUM; ++j) {
            sum += w[j];
            if (std::isnan(w[j]) || std::isinf(w[j])) all_finite = false;
        }

        TEST_CHECK(all_finite, "参数组 " + std::to_string(i) + " 所有权重非 NaN/Inf");
        TEST_CHECK(std::abs(sum - 1.0) < 0.01, "参数组 " + std::to_string(i) + " 权重之和 ≈ 1.0");
        TEST_CHECK(tn == 20, "参数组 " + std::to_string(i) + " top_n == 20");

        LOG_INFO("  参数[{}]: weights=[{:.3f}, {:.3f}, {:.3f}, {:.3f}, {:.3f}] top_n={}",
            i, w[0], w[1], w[2], w[3], w[4], tn);
    }

    // ==================== 阶段 4：回测引擎检验 ====================
    LOG_INFO("--- [阶段4] 回测引擎 ---");

    PerformanceCollector::Initialize();

    BacktestEngine engine;
    engine.Initialize(1000000.0, 0);
    TEST_CHECK(engine.IsInitialized(), "BacktestEngine 初始化成功");

    engine.Run();
    BacktestSummary s0 = engine.GetSummary();

    TEST_CHECK(s0.total_rebalances > 0, "调仓次数 > 0");
    TEST_CHECK(s0.total_trade_days > 0, "交易天数 > 0");
    TEST_CHECK(s0.final_net_value > 0.0, "最终净值 > 0");
    TEST_CHECK(s0.initial_capital == 1000000.0, "初始资金正确");

    LOG_INFO("  第一组绩效: 总收益={:.2f}%, 夏普={:.4f}, 回撤={:.2f}%, 胜率={:.2f}%",
        s0.total_return * 100, s0.sharpe_ratio, s0.max_drawdown * 100, s0.win_rate * 100);

    BacktestSummary best = s0;
    int best_idx = 0;
    for (int i = 1; i < param_count; ++i) {
        engine.ReInitialize(1000000.0, i);
        engine.Run();
        BacktestSummary s = engine.GetSummary();
        PerformanceCollector::GetPerformanceCollector()->AddSummary(s, 0);
        if (s.sharpe_ratio > best.sharpe_ratio) {
            best = s;
            best_idx = i;
        }
    }

    TEST_CHECK(!std::isnan(best.total_return) && !std::isinf(best.total_return), "最优总收益非 NaN/Inf");
    TEST_CHECK(!std::isnan(best.sharpe_ratio) && !std::isinf(best.sharpe_ratio), "最优夏普非 NaN/Inf");
    TEST_CHECK(best.max_drawdown >= 0.0 && best.max_drawdown <= 1.0, "最大回撤在 [0, 1] 范围");
    TEST_CHECK(best.win_rate >= 0.0 && best.win_rate <= 1.0, "胜率在 [0, 1] 范围");

    // ==================== 阶段 5：最优参数输出 ====================
    LOG_INFO("--- [阶段5] 最优参数 ---");

    auto best_weights = gd->GetWeights(best_idx);
    LOG_INFO("  最优参数索引: {}", best_idx);
    LOG_INFO("  权重: [{:.4f}, {:.4f}, {:.4f}, {:.4f}, {:.4f}]",
        best_weights[0], best_weights[1], best_weights[2], best_weights[3], best_weights[4]);
    LOG_INFO("  选股数: {}", gd->GetTopN(best_idx));
    LOG_INFO("  总收益: {:.2f}%", best.total_return * 100);
    LOG_INFO("  年化:   {:.2f}%", best.annual_return * 100);
    LOG_INFO("  夏普:   {:.4f}", best.sharpe_ratio);
    LOG_INFO("  回撤:   {:.2f}%", best.max_drawdown * 100);
    LOG_INFO("  胜率:   {:.2f}%", best.win_rate * 100);
    LOG_INFO("  净值:   {:.2f}", best.final_net_value);

    TEST_CHECK(!std::isnan(best.annual_return), "最优年化收益非 NaN");

    // ==================== 清理 ====================
    delete PerformanceCollector::GetPerformanceCollector();
    GlobalData::Destroy();
    std::remove("fulltest_daily.csv");
    std::remove("fulltest_daily_extended.csv");
    std::remove("fulltest_daily_financial.csv");

    // ==================== 汇总 ====================
    LOG_INFO("========================================");
    if (passed) {
        LOG_INFO("  全流程检验：全部通过！");
    } else {
        LOG_ERROR("  全流程检验：存在失败项，请检查上方 [FAIL] 信息");
    }
    LOG_INFO("========================================");
}
