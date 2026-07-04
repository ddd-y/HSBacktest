#pragma once

#include "../HSBacktest.h"
#include "../MyLog/Logger.h"
#include "../Datalevel/read_csvdata/read_csv.h"
#include "../Datalevel/Global_data.h"
#include "../Datalevel/param_builder/param_builder.h"
#include "../ConfigLvevl/configer.h"
#include "../EngineLevel/BacktestEngine.h"
#include "../CollectorLevel/performance_collector.h"
#include "../ParaLevel/multi_thread/multi_runner.h"
#include <fstream>
#include <iomanip>
#include <cmath>
#include <cstdio>
#include <string>
#include <omp.h>

// 断言宏
#define MT_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            LOG_ERROR("[FAIL] {} ({}:{})", msg, __FILE__, __LINE__); \
            mt_passed = false; \
        } else { \
            LOG_INFO("[PASS] {}", msg); \
        } \
    } while(0)

static bool mt_passed = true;

void generate_multithread_test_csv(int num_stocks = 3, int num_days = 42) {
    for (int s = 0; s < num_stocks; ++s) {
        std::string suffix = (s == 0) ? "" : "_" + std::to_string(s + 1);
        std::string base = "mttest" + suffix;

        {
            std::ofstream ofs(base + "_daily.csv");
            ofs << "trade_date,close,open,adj_factor,industry_code,is_suspended,is_delisted,is_limit_up,is_limit_down" << std::endl;
            double price = 100.0 + s * 10.0;
            for (int i = 0; i < num_days; ++i) {
                ofs << (20250101 + i) << ","
                    << std::fixed << std::setprecision(2) << price << ","
                    << (price - 0.5) << ",1.0," << ((s + 1) * 10 + i % 3) << ",0,0,0,0" << std::endl;
                price += (i % 7 == 0 ? -1.5 : 0.8 + s * 0.1);
            }
        }
        {
            std::ofstream ofs(base + "_daily_extended.csv");
            ofs << "trade_date,high,low,volume,amount" << std::endl;
            double price = 100.0 + s * 10.0;
            double vol = 1000000.0 + s * 200000.0;
            for (int i = 0; i < num_days; ++i) {
                ofs << (20250101 + i) << ","
                    << std::fixed << std::setprecision(2) << (price + 0.5) << ","
                    << (price - 0.5) << "," << vol << "," << (vol * price) << std::endl;
                price += (i % 7 == 0 ? -1.5 : 0.8 + s * 0.1);
                vol += 30000.0;
            }
        }
        {
            std::ofstream ofs(base + "_daily_financial.csv");
            ofs << "cash_dividend,split_ratio,total_shares,float_shares,eps_ttm,pe_ttm,pb_lf,roe_ttm" << std::endl;
            double eps = 5.0 - s * 0.5;
            double pe = 20.0 - s * 2.0;
            for (int i = 0; i < num_days; ++i) {
                ofs << "0.0,1.0,100000000,80000000," << eps << "," << pe << ",1.5,0.15" << std::endl;
            }
        }
    }
    LOG_INFO("生成 {} 只股票 x {} 天测试数据完成", num_stocks, num_days);
}

void multithread_runner_test()
{
    mt_passed = true;
    HSBacktest::Logger::getInstance().init("mttest");

    LOG_INFO("========================================");
    LOG_INFO("   多线程回测测试：MultiRunner");
    LOG_INFO("========================================");

    // ===== 步骤1 =====
    LOG_INFO("[1/6] 生成多只股票测试数据...");
    generate_multithread_test_csv(3, 42);

    // ===== 步骤2 =====
    LOG_INFO("[2/6] 配置参数搜索...");
    auto& search_cfg = Configer::GetParamSearchConfig();
    search_cfg.SetMode(ParamSearchConfiger::SearchMode::RANDOM);
    search_cfg.SetRandomSamples(20);
    search_cfg.SetTopNCandidates({ 10 });
    search_cfg.SetNormalizeWeights(true);

    // ===== 步骤3 =====
    LOG_INFO("[3/6] 初始化 GlobalData...");
    std::vector<std::string> stock_files = { "mttest", "mttest_2", "mttest_3" };
    GlobalData::Init(stock_files);
    GlobalData* gd = GlobalData::GetGlobalData();

    int stock_count = gd->get_stock_count();
    int param_count = gd->GetAdjustParamCount();
    int cpu_count = omp_get_num_procs();

    MT_CHECK(gd != nullptr, "GlobalData 单例不为空");
    MT_CHECK(stock_count == 3, "股票数 == 3");
    MT_CHECK(param_count == 20, "参数组数 == 20");
    MT_CHECK(cpu_count >= 1, "CPU 核心数 >= 1");

    LOG_INFO("  (股票={}, 参数组={}, 调仓日={}, CPU核心={})",
        stock_count, param_count, gd->get_rebalance_index().size(), cpu_count);

    // ===== 步骤4：单线程基准 =====
    LOG_INFO("[4/6] 单线程基准回测...");

    PerformanceCollector::Initialize();
    BacktestEngine baseline_engine;
    baseline_engine.Initialize(1000000.0, 0);
    baseline_engine.Run();
    BacktestSummary baseline = baseline_engine.GetSummary();

    MT_CHECK(!std::isnan(baseline.sharpe_ratio), "单线程基准: 夏普非 NaN");
    MT_CHECK(!std::isnan(baseline.max_drawdown), "单线程基准: 回撤非 NaN");

    LOG_INFO("  基准绩效: 年化={:.2f}%, 夏普={:.4f}, 回撤={:.2f}%",
        baseline.annual_return * 100, baseline.sharpe_ratio, baseline.max_drawdown * 100);

    // ===== 步骤5：多线程 =====
    LOG_INFO("[5/6] 多线程并行回测...");

    delete PerformanceCollector::GetPerformanceCollector();
    PerformanceCollector::Initialize();

    try {
        MultiRunner::MultiRun();
        LOG_INFO("  MultiRunner::MultiRun() 执行完成，无异常");
    } catch (const std::exception& e) {
        LOG_ERROR("  MultiRunner 异常: {}", e.what());
        mt_passed = false;
    }

    // ===== 步骤6：验证 =====
    LOG_INFO("[6/6] 验证多线程结果...");

    PerformanceCollector* pc = PerformanceCollector::GetPerformanceCollector();
    MT_CHECK(pc != nullptr, "PerformanceCollector 不为空");

    int total_summaries = pc->GetTotalSummaryCount();
    MT_CHECK(total_summaries == param_count,
        "多线程收集结果数 (" + std::to_string(total_summaries) + ") == 参数组数 (" + std::to_string(param_count) + ")");

    // 单线程重跑所有参数组，确认结果合理性
    LOG_INFO("--- 单线程对比验证 ---");

    int results_ok = 0;
    int results_fail = 0;
    BacktestSummary best_st;
    int best_st_idx = -1;

    for (int i = 0; i < param_count; ++i) {
        BacktestEngine engine;
        engine.Initialize(1000000.0, i);
        engine.Run();
        BacktestSummary s = engine.GetSummary();

        bool ok = true;
        ok &= !std::isnan(s.annual_return) && !std::isinf(s.annual_return);
        ok &= !std::isnan(s.sharpe_ratio) && !std::isinf(s.sharpe_ratio);
        ok &= s.max_drawdown >= 0.0 && s.max_drawdown <= 1.0;

        if (ok) results_ok++; else results_fail++;

        if (i == 0 || s.sharpe_ratio > best_st.sharpe_ratio) {
            best_st = s;
            best_st_idx = i;
        }

        LOG_INFO("  [{:2d}] 年化={:8.2f}%  夏普={:7.4f}  回撤={:7.2f}%",
            i, s.annual_return * 100, s.sharpe_ratio, s.max_drawdown * 100);
    }

    MT_CHECK(results_fail == 0, "所有参数组绩效数据合理 (OK=" + std::to_string(results_ok) + ")");
    MT_CHECK(best_st_idx >= 0, "找到了最优参数组");

    // 打印最优
    LOG_INFO("--- 最优参数 ---");
    LOG_INFO("  参数索引: {}", best_st_idx);
    auto bw = gd->GetWeights(best_st_idx);
    LOG_INFO("  权重: [{:.3f}, {:.3f}, {:.3f}, {:.3f}, {:.3f}]", bw[0], bw[1], bw[2], bw[3], bw[4]);
    LOG_INFO("  top_n: {}", gd->GetTopN(best_st_idx));
    LOG_INFO("  年化:   {:.2f}%", best_st.annual_return * 100);
    LOG_INFO("  夏普:   {:.4f}", best_st.sharpe_ratio);
    LOG_INFO("  回撤:   {:.2f}%", best_st.max_drawdown * 100);

    // ===== 清理 =====
    delete PerformanceCollector::GetPerformanceCollector();
    GlobalData::Destroy();
    std::remove("mttest_daily.csv");
    std::remove("mttest_daily_extended.csv");
    std::remove("mttest_daily_financial.csv");
    std::remove("mttest_2_daily.csv");
    std::remove("mttest_2_daily_extended.csv");
    std::remove("mttest_2_daily_financial.csv");
    std::remove("mttest_3_daily.csv");
    std::remove("mttest_3_daily_extended.csv");
    std::remove("mttest_3_daily_financial.csv");

    // ===== 汇总 =====
    LOG_INFO("========================================");
    if (mt_passed) {
        LOG_INFO("  多线程回测测试：全部通过！");
    } else {
        LOG_ERROR("  多线程回测测试：存在失败项");
    }
    LOG_INFO("========================================");
}
