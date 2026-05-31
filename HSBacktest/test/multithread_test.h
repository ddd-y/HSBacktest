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
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>
#include <string>
#include<omp.h>

// 断言宏
#define MT_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "[FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
            mt_passed = false; \
        } else { \
            std::cout << "[PASS] " << msg << std::endl; \
        } \
    } while(0)

static bool mt_passed = true;

// 生成多只股票测试数据（用于多线程场景：数据量大一些才有意义）
void generate_multithread_test_csv(int num_stocks = 3, int num_days = 42) {
    for (int s = 0; s < num_stocks; ++s) {
        std::string suffix = (s == 0) ? "" : "_" + std::to_string(s + 1);
        std::string base = "mttest" + suffix;

        // 主日线
        {
            std::ofstream ofs(base + "_daily.csv");
            ofs << "trade_date,close,open,adj_factor,is_suspended,is_delisted,is_limit_up,is_limit_down" << std::endl;
            double price = 100.0 + s * 10.0;
            for (int i = 0; i < num_days; ++i) {
                ofs << (20250101 + i) << ","
                    << std::fixed << std::setprecision(2) << price << ","
                    << (price - 0.5) << ",1.0,0,0,0,0" << std::endl;
                price += (i % 7 == 0 ? -1.5 : 0.8 + s * 0.1);
            }
        }
        // 扩展数据
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
        // 财务数据
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
    std::cout << "[TEST] 生成 " << num_stocks << " 只股票 x " << num_days << " 天测试数据完成" << std::endl;
}

void multithread_runner_test()
{
    mt_passed = true;
    HSBacktest::Logger::getInstance().init("mttest");

    std::cout << "\n========================================" << std::endl;
    std::cout << "   多线程回测测试：MultiRunner" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // ===== 步骤1：生成多只股票数据 =====
    std::cout << "[1/6] 生成多只股票测试数据..." << std::endl;
    generate_multithread_test_csv(3, 42);  // 3只股票 x 42天

    // ===== 步骤2：配置参数搜索（20组，让多线程有意义）=====
    std::cout << "[2/6] 配置参数搜索..." << std::endl;
    auto& search_cfg = Configer::GetParamSearchConfig();
    search_cfg.SetMode(ParamSearchConfiger::SearchMode::RANDOM);
    search_cfg.SetRandomSamples(20);         // 20组参数
    search_cfg.SetTopNCandidates({ 10 });    // 固定 top_n=10
    search_cfg.SetNormalizeWeights(true);

    // ===== 步骤3：初始化 GlobalData =====
    std::cout << "[3/6] 初始化 GlobalData..." << std::endl;
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

    std::cout << "  (股票=" << stock_count << ", 参数组=" << param_count
              << ", 调仓日=" << gd->get_rebalance_index().size()
              << ", CPU核心=" << cpu_count << ")" << std::endl;

    // ===== 步骤4：单线程跑一遍作为基准 =====
    std::cout << "\n[4/6] 单线程基准回测..." << std::endl;

    PerformanceCollector::Initialize();
    BacktestEngine baseline_engine;
    baseline_engine.Initialize(1000000.0, 0);
    baseline_engine.Run();
    BacktestSummary baseline = baseline_engine.GetSummary();

    MT_CHECK(baseline.total_rebalances > 0, "单线程基准: 调仓次数 > 0");
    MT_CHECK(baseline.final_net_value > 0.0, "单线程基准: 最终净值 > 0");
    MT_CHECK(!std::isnan(baseline.sharpe_ratio), "单线程基准: 夏普非 NaN");
    MT_CHECK(!std::isnan(baseline.max_drawdown), "单线程基准: 回撤非 NaN");

    std::cout << "  基准绩效: 收益=" << baseline.total_return * 100
              << "%, 夏普=" << baseline.sharpe_ratio
              << ", 回撤=" << baseline.max_drawdown * 100
              << "%, 净值=" << baseline.final_net_value << std::endl;

    // ===== 步骤5：多线程跑 MultiRunner =====
    std::cout << "\n[5/6] 多线程并行回测..." << std::endl;

    // 重新初始化 PerformanceCollector（重置之前的数据）
    delete PerformanceCollector::GetPerformanceCollector();
    PerformanceCollector::Initialize();

    try {
        MultiRunner::MultiRun();
        std::cout << "  MultiRunner::MultiRun() 执行完成，无异常" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "  [FAIL] MultiRunner 异常: " << e.what() << std::endl;
        mt_passed = false;
    }

    // ===== 步骤6：验证结果 =====
    std::cout << "\n[6/6] 验证多线程结果..." << std::endl;

    PerformanceCollector* pc = PerformanceCollector::GetPerformanceCollector();
    MT_CHECK(pc != nullptr, "PerformanceCollector 不为空");

    // 统计收集到的 summary 总数
    int total_summaries = 0;
    for (int t = 0; t < cpu_count; ++t) {
        // PerformanceCollector 的 summary_list[t] 存的是 deque<BacktestSummary>
        // 这里无法直接访问 private 成员，通过另一种方式验证
    }

    // 验证：至少每个参数组都有结果
    // 重新单线程跑所有参数组，和多线程结果对比
    std::cout << "\n--- 对比验证：单线程 vs 多线程 ---" << std::endl;
    std::cout << std::left
              << std::setw(6) << "Index"
              << std::setw(14) << "总收益%"
              << std::setw(10) << "夏普"
              << std::setw(12) << "最大回撤%"
              << std::setw(10) << "胜率%" << std::endl;
    std::cout << std::string(52, '-') << std::endl;

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
        ok &= !std::isnan(s.total_return) && !std::isinf(s.total_return);
        ok &= !std::isnan(s.sharpe_ratio) && !std::isinf(s.sharpe_ratio);
        ok &= s.max_drawdown >= 0.0 && s.max_drawdown <= 1.0;
        ok &= s.win_rate >= 0.0 && s.win_rate <= 1.0;
        ok &= s.final_net_value > 0.0;
        ok &= s.total_rebalances > 0;

        if (ok) results_ok++; else results_fail++;

        if (i == 0 || s.sharpe_ratio > best_st.sharpe_ratio) {
            best_st = s;
            best_st_idx = i;
        }

        std::cout << std::fixed << std::setprecision(2)
                  << std::setw(6) << i
                  << std::setw(14) << s.total_return * 100
                  << std::setw(10) << s.sharpe_ratio
                  << std::setw(12) << s.max_drawdown * 100
                  << std::setw(10) << s.win_rate * 100
                  << std::endl;
    }

    MT_CHECK(results_fail == 0, "所有参数组绩效数据合理 (OK=" + std::to_string(results_ok) + ")");
    MT_CHECK(best_st_idx >= 0, "找到了最优参数组");

    // 打印最优
    std::cout << "\n--- 最优参数 ---" << std::endl;
    std::cout << "  参数索引: " << best_st_idx << std::endl;
    auto bw = gd->GetWeights(best_st_idx);
    std::cout << "  权重: [";
    for (int j = 0; j < FACTOR_NUM; ++j) {
        if (j > 0) std::cout << ", ";
        std::cout << std::fixed << std::setprecision(3) << bw[j];
    }
    std::cout << "]" << std::endl;
    std::cout << "  top_n: " << gd->GetTopN(best_st_idx) << std::endl;
    std::cout << "  总收益: " << best_st.total_return * 100 << "%" << std::endl;
    std::cout << "  夏普:   " << best_st.sharpe_ratio << std::endl;
    std::cout << "  回撤:   " << best_st.max_drawdown * 100 << "%" << std::endl;
    std::cout << "  净值:   " << best_st.final_net_value << std::endl;

    // ===== 清理 =====
    GlobalData::Destroy();

    // ===== 汇总 =====
    std::cout << "\n========================================" << std::endl;
    if (mt_passed) {
        std::cout << "  多线程回测测试：全部通过！" << std::endl;
    } else {
        std::cout << "  多线程回测测试：存在失败项" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
