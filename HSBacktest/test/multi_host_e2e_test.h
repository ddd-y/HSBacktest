#pragma once
// ==========================================
// multi_host_e2e_test.h — HostManager 端到端测试
//
// 原理：
//   用同一批合成数据（2股×42天），分别跑单机和 MPI 两种模式，
//   对比 PerformanceCollector 中收集到的 BacktestSummary 集合。
//   如果所有字段完全一致 → HostManager 正确。
//
// 需要 mpirun（MPI_Init 依赖 mpirun 环境）：
//   mpirun -n 1 ./HSBacktest    # 生成基准文件
//   mpirun -n 4 ./HSBacktest    # MPI 模式，与基准对比
//
// 使用方法: 在 main() 中调用 run_multi_host_e2e_test()
// ==========================================

#include "../ParaLevel/multi_host/multi_host.h"
#include "../ParaLevel/multi_thread/multi_runner.h"
#include "../Datalevel/Global_data.h"
#include "../Datalevel/param_builder/param_builder.h"
#include "../CollectorLevel/performance_collector.h"
#include "../CollectorLevel/BacktestSummary.h"
#include "../ConfigLvevl/configer.h"
#include "../MyLog/Logger.h"
#include <fstream>
#include <set>
#include <cstdio>

#ifndef NDEBUG
// 与 backtest_unit_test.h 共享的合成数据生成逻辑
// 这里内联一份，避免头文件依赖
namespace {
    void mpi_e2e_create_data() {
        const int T = 42;
        const char* codes[] = {"000001", "000002"};
        auto price0 = [](int d) {
            if (d <= 20) return 10.0 + d * 0.5;
            if (d <= 40) return 20.0;
            return 30.0;
        };
        auto price1 = [](int d) {
            if (d <= 20) return 10.0 - d * 0.25;
            if (d <= 40) return 5.0;
            return 3.0;
        };
        for (int s = 0; s < 2; ++s) {
            auto pf = (s == 0) ? price0 : price1;
            {
                std::ofstream f(std::string(codes[s]) + "_daily.csv");
                f << "trade_date,close,open,adj_factor,industry_code,"
                     "is_suspended,is_delisted,is_limit_up,is_limit_down\n";
                for (int d = 0; d < T; ++d) {
                    double p = pf(d);
                    f << (20250101 + d) << "," << std::fixed
                      << std::setprecision(2) << p << "," << p
                      << ",1.0," << (s + 1) << ",0,0,0,0\n";
                }
            }
            {
                std::ofstream f(std::string(codes[s]) + "_daily_extended.csv");
                f << "high,low,volume,amount\n";
                for (int d = 0; d < T; ++d) {
                    double p = pf(d);
                    f << std::fixed << std::setprecision(2) << p << "," << p
                      << ",1000000," << (p * 1000000) << "\n";
                }
            }
            {
                std::ofstream f(std::string(codes[s]) + "_daily_financial.csv");
                f << "cash_dividend,split_ratio,total_shares,float_shares,"
                     "eps_ttm,pe_ttm,pb_lf,roe_ttm\n";
                for (int d = 0; d < T; ++d)
                    f << "0.0,1.0,100000000,80000000,5.0,20.0,1.5,0.15\n";
            }
        }
    }

    void mpi_e2e_write_config(int n_groups) {
        std::ofstream f("test_mpi_config.json");
        f << "{\n"
          << "  \"data\": { \"stock_files\": [\"000001\", \"000002\"] },\n"
          << "  \"strategy\": {\n"
          << "    \"hold_days\": 20, \"top_n\": 1,\n"
          << "    \"single_position_limit\": 0.5,\n"
          << "    \"industry_position_limit\": 0.2,\n"
          << "    \"single_stock_stop_loss\": 0.1,\n"
          << "    \"single_stock_take_profit\": 0.3,\n"
          << "    \"risk_free_rate\": 0.03\n"
          << "  },\n"
          << "  \"transaction_cost\": {\n"
          << "    \"commission_rate\": 0.0003, \"min_commission\": 5.0,\n"
          << "    \"stamp_duty_rate\": 0.001, \"transfer_fee_rate\": 0.00002,\n"
          << "    \"buy_slippage_rate\": 0.001, \"sell_slippage_rate\": 0.0015\n"
          << "  },\n"
          << "  \"param_search\": {\n"
          << "    \"mode\": \"GRID\",\n"
          << "    \"grid_step\": 1.0,\n"
          << "    \"top_n_candidates\": [1],\n"
          << "    \"normalize_weights\": true\n"
          << "  },\n"
          << "  \"system\": { \"use_mpi\": true, \"init_capital\": 100000.0 }\n"
          << "}\n";
    }

    // 把 BacktestSummary 序列化成可比较的字符串
    std::string summary_to_key(const BacktestSummary& s) {
        char buf[448];
        std::snprintf(buf, sizeof(buf),
            "p%d|%.6f|%.6f|%.6f|%.6f|%.6f|%.6f",
            s.param_index,
            s.total_return, s.annual_return, s.annual_volatility,
            s.sharpe_ratio, s.max_drawdown,
            s.avg_turnover);
        return std::string(buf);
    }

    const char* GOLDEN_FILE = "test_mpi_golden.txt";

    void save_golden(const std::vector<BacktestSummary>& results) {
        std::ofstream f(GOLDEN_FILE);
        // 先写数量
        f << results.size() << "\n";
        for (auto& s : results)
            f << summary_to_key(s) << "\n";
        LOG_INFO("Golden: saved {} summaries to {}", results.size(), GOLDEN_FILE);
    }

    std::vector<std::string> load_golden() {
        std::ifstream f(GOLDEN_FILE);
        std::vector<std::string> lines;
        std::string line;
        // 跳过第一行（数量）
        std::getline(f, line);  // count
        while (std::getline(f, line)) {
            if (!line.empty()) lines.push_back(line);
        }
        return lines;
    }
}

// ==========================================
// 主测试入口
// ==========================================
inline void run_multi_host_e2e_test()
{
    // ---- 0. 先初始化 MPI（之后才知道 rank/size）----
    HostManager mgr;
    mgr.InitMPIRelated();
    int rank = mgr.GetRank();
    int size = mgr.GetRankSize();

    // ---- 1. 只有 rank 0 创建合成数据文件和配置（避免多进程写冲突）----
    if (rank == 0) {
        mpi_e2e_create_data();
        mpi_e2e_write_config(0);
    }
    MPI_Barrier(MPI_COMM_WORLD);  // 等待文件就绪

    // ---- 2. 所有进程加载数据 ----
    Configer::LoadFromFile("test_mpi_config.json");
    HSBacktest::Logger::getInstance().init("test_log.txt");
    std::vector<std::string> codes = {"000001", "000002"};
    GlobalData::Init(codes);
    PerformanceCollector::Initialize();

    // 生成足够多的参数组合，确保产生多个 task chunk
    // UNIT_TASK_SIZE=512，这里生成 2000 组 → 4 个 task [0,512),[512,1024),[1024,1536),[1536,2000)
    // 配合 mpirun -n 4，每个 rank 分到 1 个 task，干完后互相偷
    std::vector<AdjustParam> custom_params;
    // 1536 参数 → 3 个 task（512×3），4 个 rank → rank3 分不到活
    // 必须从 rank0/1/2 偷 → 真正触发 work stealing
    const int N = 1536;
    for (int i = 0; i < N; ++i) {
        AdjustParam ap;
        double a = (i % 5 == 0) ? 0.5 : 0.125;
        double b = (i % 5 == 1) ? 0.5 : 0.125;
        double c = (i % 5 == 2) ? 0.5 : 0.125;
        double d = (i % 5 == 3) ? 0.5 : 0.125;
        double e = (i % 5 == 4) ? 0.5 : 0.125;
        double sum = a + b + c + d + e;
        ap.factor_weights = {{a / sum, b / sum, c / sum, d / sum, e / sum}};
        ap.top_n = 1;
        custom_params.push_back(ap);
    }
    GlobalData::GetGlobalData()->SetCustomAdjustParams(custom_params);

    int total_params = GlobalData::GetGlobalData()->GetAdjustParamCount();
    LOG_INFO("MPI E2E: {} parameter combinations to test", total_params);

    if (size == 1) {
        // ═══════════════════════════════════════
        // 单进程模式：生成基准文件
        // ═══════════════════════════════════════
        LOG_INFO("MPI E2E: single-process mode → generating golden file");
        MultiRunner::MultiRun();
        auto results = PerformanceCollector::GetPerformanceCollector()
                           ->TakeAllSummaries();
        save_golden(results);
        LOG_INFO("MPI E2E: golden generation complete. "
                 "Now run: mpirun -n 4 ./HSBacktest");

        // 清理临时文件
        for (auto& code : codes)
            for (auto& sfx : {"_daily.csv", "_daily_extended.csv", "_daily_financial.csv"})
                std::remove((code + sfx).c_str());
        std::remove("test_mpi_config.json");

    } else {
        // ═══════════════════════════════════════
        // 多进程模式：工作窃取 → 对比基准
        // ═══════════════════════════════════════
        LOG_INFO("MPI E2E: multi-process mode (rank {}/{})", rank, size);

        mgr.distribute_task();   // work-stealing + gather

        if (rank == 0) {
            auto results = PerformanceCollector::GetPerformanceCollector()
                               ->TakeAllSummaries();

            // 收集到的结果数
            LOG_INFO("MPI E2E: gathered {} summaries from {} ranks",
                     results.size(), size);

            // 加载基准
            auto golden_lines = load_golden();
            if (golden_lines.empty()) {
                LOG_ERROR("MPI E2E: golden file {} not found or empty. "
                          "Run with mpirun -n 1 first.", GOLDEN_FILE);
            } else {
                // 构建 actual 集合
                std::set<std::string> actual_set;
                for (auto& s : results)
                    actual_set.insert(summary_to_key(s));

                // 构建 expected 集合
                std::set<std::string> expected_set(
                    golden_lines.begin(), golden_lines.end());

                // 对比
                bool match = (actual_set.size() == expected_set.size());
                if (match) {
                    for (auto& k : expected_set) {
                        if (!actual_set.count(k)) { match = false; break; }
                    }
                }

                if (match) {
                    LOG_INFO("MPI E2E PASS: {} summaries match golden exactly",
                             actual_set.size());
                    // 额外输出到 stderr，确保可见
                    fprintf(stderr, "\n======== MPI E2E PASS ========\n\n");
                } else {
                    LOG_ERROR("MPI E2E FAIL: mismatch! "
                              "actual={}, expected={}",
                              actual_set.size(), expected_set.size());
                    fprintf(stderr, "\n======== MPI E2E FAIL ========\n\n");

                    // 打印缺失和多余的
                    for (auto& k : expected_set)
                        if (!actual_set.count(k))
                            LOG_ERROR("  MISSING: {}", k);
                    for (auto& k : actual_set)
                        if (!expected_set.count(k))
                            LOG_ERROR("  EXTRA:   {}", k);
                }
            }

            // 清理
            for (auto& code : codes)
                for (auto& sfx : {"_daily.csv", "_daily_extended.csv", "_daily_financial.csv"})
                    std::remove((code + sfx).c_str());
            std::remove("test_mpi_config.json");
        }
    }

    GlobalData::Destroy();
}
#endif // NDEBUG