#pragma once
// ==========================================
// backtest_unit_test.h — 42天迷你回测单元测试
//
// 用合成数据（2只股票 × 42天）跑一遍完整回测，
// 结果可手工验算：
//   - 股票A: 价格 10→20（前21天），day41涨到30
//   - 股票B: 价格 10→5（前21天），day41跌到3
//   - day21调仓选股A，等权买入
//   - day41清仓
//
// 手工预期: final_nav ≈ 123733.73
//
// 运行:
//   在 main() 中 #include "test/backtest_unit_test.h"
//   然后调用 run_backtest_unit_test()
// ==========================================

#include "../HSBacktest.h"
#include "../MyLog/Logger.h"
#include "../Datalevel/Global_data.h"
#include "../Datalevel/param_builder/param_builder.h"
#include "../EngineLevel/BacktestEngine.h"
#include "../CollectorLevel/performance_collector.h"
#include "../ConfigLvevl/configer.h"
#include <fstream>
#include <cmath>
#include <cstdio>

// --- 断言宏 ---
#define BT_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            LOG_ERROR("[BT-FAIL] {} ({}:{})", msg, __FILE__, __LINE__); \
            bt_all_passed = false; \
        } else { \
            LOG_INFO("[BT-PASS] {}", msg); \
        } \
    } while(0)

static bool bt_all_passed = true;

// ==========================================
// 生成42天合成CSV数据
// ==========================================
void create_synthetic_data()
{
    const int T = 42;
    const char* codes[] = {"000001", "000002"};

    auto price0 = [](int d) -> double {
        if (d <= 20) return 10.0 + d * 0.5;
        if (d <= 40) return 20.0;
        return 30.0;
    };
    auto price1 = [](int d) -> double {
        if (d <= 20) return 10.0 - d * 0.25;
        if (d <= 40) return 5.0;
        return 3.0;
    };

    for (int s = 0; s < 2; ++s) {
        auto pf = (s == 0) ? price0 : price1;

        // daily.csv
        {
            std::ofstream f(std::string(codes[s]) + "_daily.csv");
            f << "trade_date,close,open,adj_factor,industry_code,"
                 "is_suspended,is_delisted,is_limit_up,is_limit_down\n";
            for (int d = 0; d < T; ++d) {
                double p = pf(d);
                f << (20250101 + d) << ","
                  << std::fixed << std::setprecision(2) << p << ","
                  << std::fixed << std::setprecision(2) << p << ","
                  << "1.0," << (s + 1) << ",0,0,0,0\n";
            }
        }

        // daily_extended.csv
        {
            std::ofstream f(std::string(codes[s]) + "_daily_extended.csv");
            f << "high,low,volume,amount\n";
            for (int d = 0; d < T; ++d) {
                double p = pf(d);
                f << std::fixed << std::setprecision(2) << p << ","
                  << std::fixed << std::setprecision(2) << p << ","
                  << "1000000," << std::fixed << std::setprecision(2) << (p*1000000) << "\n";
            }
        }

        // daily_financial.csv
        {
            std::ofstream f(std::string(codes[s]) + "_daily_financial.csv");
            f << "cash_dividend,split_ratio,total_shares,float_shares,"
                 "eps_ttm,pe_ttm,pb_lf,roe_ttm\n";
            for (int d = 0; d < T; ++d)
                f << "0.0,1.0,100000000,80000000,5.0,20.0,1.5,0.15\n";
        }
    }
}

// ==========================================
// 手工计算预期最终净值
//
// 约束链：
//   行业仓位上限 20% → 最多投入 20000 → 1000股
//   单票上限 50% → 最多 50000（比行业上限宽松，不触发）
//   等权目标 → 100000（唯一选中，全仓但受限于行业上限）
// ==========================================
double hand_calc_final_nav()
{
    const double init_cap = 100000.0;
    const double p_buy = 20.0;     // day21 close of stock A
    const double p_sell = 30.0;    // day41 close of stock A

    // day21 buy: 受行业上限 20% 约束 → 20000，取整 1000 股
    int shares = static_cast<int>(init_cap * 0.2 / p_buy);  // 20000/20=1000
    shares = (shares / 100) * 100;                            // 1000

    // C++ 引擎的 CalcTransactionCost：buy at raw price + cost
    double buy_notional = shares * p_buy;                                    // 20000
    double comm_buy = std::max(buy_notional * 0.0003, 5.0);                 // 6
    double slip_buy = buy_notional * 0.001;                                  // 20
    double trans_buy = buy_notional * 0.00002;                              // 0.4
    double total_buy = buy_notional + comm_buy + slip_buy + trans_buy;      // 20026.40
    double cash_after = init_cap - total_buy;                                // 79973.60

    // day41 sell
    double sell_notional = shares * p_sell;                                  // 30000
    double comm_sell = std::max(sell_notional * 0.0003, 5.0);              // 9
    double stamp_sell = sell_notional * 0.001;                              // 30
    double slip_sell = sell_notional * 0.0015;                              // 45
    double trans_sell = sell_notional * 0.00002;                            // 0.6
    double net_proceeds = sell_notional - comm_sell - stamp_sell
                          - slip_sell - trans_sell;                         // 29915.40

    return cash_after + net_proceeds;                                        // 109889.00
}

// ==========================================
// 测试入口
// ==========================================
inline void run_backtest_unit_test()
{
    HSBacktest::Logger::getInstance().init("test_log.txt");
    LOG_INFO("========== 42天迷你回测测试 ==========");

    // 1. 在当前目录生成合成数据
    create_synthetic_data();

    // 2. 写 config.json（匹配 Configer 的 JSON 结构）
    {
        std::ofstream f("test_config.json");
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
          << "    \"momentum_weight_min\": 0.2, \"momentum_weight_max\": 0.2,\n"
          << "    \"turnover_weight_min\": 0.2, \"turnover_weight_max\": 0.2,\n"
          << "    \"volatility_weight_min\": 0.2, \"volatility_weight_max\": 0.2,\n"
          << "    \"mcap_weight_min\": 0.2, \"mcap_weight_max\": 0.2,\n"
          << "    \"ep_weight_min\": 0.2, \"ep_weight_max\": 0.2,\n"
          << "    \"grid_step\": 1.0,\n"
          << "    \"top_n_candidates\": [1],\n"
          << "    \"normalize_weights\": true\n"
          << "  },\n"
          << "  \"system\": { \"use_mpi\": false, \"init_capital\": 100000.0 }\n"
          << "}\n";
    }

    // 3. 加载配置
    Configer::LoadFromFile("test_config.json");

    // 4. 初始化全局数据（stock code 列表）
    std::vector<std::string> codes = {"000001", "000002"};
    GlobalData::Init(codes);

    // 5. 性能收集器
    PerformanceCollector::Initialize();

    // 6. 覆盖参数（等权, top_n=1）
    std::vector<AdjustParam> custom_params;
    AdjustParam ap;
    ap.factor_weights = {{0.2, 0.2, 0.2, 0.2, 0.2}};
    ap.top_n = 1;
    custom_params.push_back(ap);
    GlobalData::GetGlobalData()->SetCustomAdjustParams(custom_params);

    // 7. 运行回测
    BacktestEngine engine;
    engine.Initialize(100000.0, 0);
    engine.Run();

    const auto& summary = engine.GetSummary();
    double expected_nav = hand_calc_final_nav();
    double expected_total_return = (expected_nav - 100000.0) / 100000.0;

    LOG_INFO("=== 回测结果 ===");
    LOG_INFO("  初始资金: 100000.00");
    LOG_INFO("  总收益率: {:.4f}%", summary.total_return * 100);
    LOG_INFO("  年化收益: {:.4f}%", summary.annual_return * 100);
    LOG_INFO("  夏普比率: {:.4f}", summary.sharpe_ratio);
    LOG_INFO("  最大回撤: {:.4f}%", summary.max_drawdown * 100);
    LOG_INFO("  手工预期净值: {:.2f}", expected_nav);
    LOG_INFO("  手工预期总收益: {:.4f}%", expected_total_return * 100);

    // 8. 断言
    BT_CHECK(std::abs(summary.total_return - expected_total_return) < 0.001,
        "总收益匹配 (实际=" + std::to_string(summary.total_return * 100)
        + "%, 预期=" + std::to_string(expected_total_return * 100) + "%)");
    BT_CHECK(!std::isnan(summary.sharpe_ratio) && !std::isinf(summary.sharpe_ratio),
        "夏普比率非 NaN/Inf");
    BT_CHECK(summary.max_drawdown >= 0.0 && summary.max_drawdown <= 1.0,
        "最大回撤在 [0, 1]");

    // 9. 清理
    GlobalData::Destroy();
    for (const auto& code : codes) {
        for (const auto& sfx : {"_daily.csv", "_daily_extended.csv", "_daily_financial.csv"})
            std::remove((code + sfx).c_str());
    }
    std::remove("test_config.json");

    if (bt_all_passed)
        LOG_INFO("========== 全部通过! ==========");
    else
        LOG_ERROR("========== 存在失败项! ==========");
}
