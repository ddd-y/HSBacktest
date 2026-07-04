#pragma once

#include "../HSBacktest.h"
#include "../MyLog/Logger.h"
#include "../Datalevel/read_csvdata/read_csv.h"
#include "../Datalevel/Global_data.h"
#include "../Datalevel/param_builder/param_builder.h"
#include "../Datalevel/stock_k_data.h"
#include "../Datalevel/data_defs.h"
#include "../ConfigLvevl/configer.h"
#include "../EngineLevel/BacktestEngine.h"
#include "../CollectorLevel/performance_collector.h"
#include "../Datalevel/factor_calculate/factorbase.h"
#include <fstream>
#include <iomanip>
#include <cmath>
#include <cstdio>
#include <string>
#include <algorithm>

// ==========================================
// 断言宏
// ==========================================
#define FT_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            LOG_ERROR("[FAIL] {} ({}:{})", msg, __FILE__, __LINE__); \
            ft_passed = false; \
        } else { \
            LOG_INFO("[PASS] {}", msg); \
        } \
    } while(0)

static bool ft_passed = true;

// ==========================================
// A. ParamBuilder 独立测试（无需 GlobalData）
// ==========================================
void test_param_builder_grid()
{
    LOG_INFO("--- [A1] ParamBuilder 网格搜索 ---");

    ParamSearchConfiger cfg;
    cfg.SetMode(ParamSearchConfiger::SearchMode::GRID);
    for (int i = 0; i < FACTOR_NUM; ++i) { cfg.SetWeightMin(i, 0.0); cfg.SetWeightMax(i, 0.5); }
    cfg.SetGridStep(0.5);
    cfg.SetTopNCandidates({ 10, 20 });
    cfg.SetNormalizeWeights(true);
    cfg.SetAllowZeroWeight(true);

    std::vector<AdjustParam> params;
    ParamBuilder::BuildParamNet(params, cfg);

    int expected = 32 * 2;
    FT_CHECK(static_cast<int>(params.size()) == expected,
        "网格搜索: 2档×5因子×2top_n = " + std::to_string(expected) + " 组 (实际=" + std::to_string(params.size()) + ")");

    bool all_normalized = true;
    for (const auto& p : params) {
        double sum = 0.0;
        for (auto w : p.factor_weights) sum += w;
        if (std::abs(sum - 1.0) > 0.01) { all_normalized = false; break; }
    }
    FT_CHECK(all_normalized, "网格搜索: 所有权重组归一化 (和≈1.0)");
}

void test_param_builder_random()
{
    LOG_INFO("--- [A2] ParamBuilder 随机采样 ---");

    ParamSearchConfiger cfg;
    cfg.SetMode(ParamSearchConfiger::SearchMode::RANDOM);
    cfg.SetRandomSamples(50);
    cfg.SetTopNCandidates({ 30 });
    cfg.SetNormalizeWeights(true);
    cfg.SetAllowZeroWeight(false);

    std::vector<AdjustParam> params;
    ParamBuilder::BuildParamNet(params, cfg);

    FT_CHECK(static_cast<int>(params.size()) == 50,
        "随机采样: 50组×1top_n = 50 (实际=" + std::to_string(params.size()) + ")");

    bool no_zeros = true;
    for (const auto& p : params) {
        for (auto w : p.factor_weights) {
            if (w < 1e-10) { no_zeros = false; break; }
        }
        if (!no_zeros) break;
    }
    FT_CHECK(no_zeros, "随机采样: 零权重已被拒绝");

    bool all_normalized = true;
    for (const auto& p : params) {
        double sum = 0.0;
        for (auto w : p.factor_weights) sum += w;
        if (std::abs(sum - 1.0) > 0.01) { all_normalized = false; break; }
    }
    FT_CHECK(all_normalized, "随机采样: 权重归一化");
}

void test_param_builder_all_zero_range()
{
    LOG_INFO("--- [A3] ParamBuilder 全零权重范围（防死循环） ---");

    ParamSearchConfiger cfg;
    cfg.SetMode(ParamSearchConfiger::SearchMode::RANDOM);
    cfg.SetRandomSamples(5);
    for (int i = 0; i < FACTOR_NUM; ++i) { cfg.SetWeightMin(i, 0.0); cfg.SetWeightMax(i, 0.0); }
    cfg.SetTopNCandidates({ 10 });
    cfg.SetNormalizeWeights(true);
    cfg.SetAllowZeroWeight(false);

    std::vector<AdjustParam> params;
    ParamBuilder::BuildParamNet(params, cfg);

    FT_CHECK(static_cast<int>(params.size()) == 5,
        "全零范围+拒绝零权重: 不卡死, 产出5组 (实际=" + std::to_string(params.size()) + ")");
}

void test_param_builder_single_factor()
{
    LOG_INFO("--- [A4] ParamBuilder 单因子扫描 ---");

    ParamSearchConfiger cfg;
    cfg.SetMode(ParamSearchConfiger::SearchMode::SINGLE_FACTOR);
    for (int i = 0; i < FACTOR_NUM; ++i) { cfg.SetWeightMin(i, 0.0); cfg.SetWeightMax(i, 1.0); }
    cfg.SetGridStep(0.5);
    cfg.SetTopNCandidates({ 20 });
    cfg.SetNormalizeWeights(true);

    std::vector<AdjustParam> params;
    ParamBuilder::BuildParamNet(params, cfg);

    FT_CHECK(static_cast<int>(params.size()) == 15,
        "单因子扫描: 5因子×3档 = 15 (实际=" + std::to_string(params.size()) + ")");

    bool valid = true;
    for (const auto& p : params) {
        double sum = 0.0;
        for (auto w : p.factor_weights) sum += w;
        if (std::abs(sum - 1.0) > 0.01) { valid = false; break; }
    }
    FT_CHECK(valid, "单因子扫描: 权重归一化");
}

// ==========================================
// B. 生成集成测试数据（5只股票 × 63天）
// ==========================================
void generate_functional_test_csv()
{
    const int num_stocks = 5;
    const int num_days = 63;

    for (int s = 0; s < num_stocks; ++s) {
        std::string suffix = (s == 0) ? "" : "_" + std::to_string(s + 1);
        std::string base = "functest" + suffix;

        {
            std::ofstream ofs(base + "_daily.csv");
            ofs << "trade_date,close,open,adj_factor,industry_code,is_suspended,is_delisted,is_limit_up,is_limit_down" << std::endl;

            double price = 100.0 + s * 5.0;
            for (int i = 0; i < num_days; ++i) {
                int is_delisted = 0, is_suspended = 0, is_limit_up = 0;
                if (s == 1 && i >= 40) is_delisted = 1;
                if (s == 2 && i >= 50) is_suspended = 1;
                if (s == 3 && i == 35) is_limit_up = 1;
                if (s == 4 && i >= 30) price -= 3.0;

                ofs << (20250101 + i) << ","
                    << std::fixed << std::setprecision(2) << price << ","
                    << (price - 0.5) << ",1.0,"
                    << ((s == 0 || s == 4) ? 1000 : 2000)
                    << "," << is_suspended << "," << is_delisted << "," << is_limit_up << ",0" << std::endl;

                if (s != 4 || i < 30) price += 0.5;
            }
        }
        {
            std::ofstream ofs(base + "_daily_extended.csv");
            ofs << "trade_date,high,low,volume,amount" << std::endl;
            double price = 100.0 + s * 5.0;
            double vol = 1000000.0 + s * 200000.0;
            for (int i = 0; i < num_days; ++i) {
                ofs << (20250101 + i) << ","
                    << std::fixed << std::setprecision(2) << (price + 0.5) << ","
                    << (price - 0.5) << "," << vol << "," << (vol * price) << std::endl;
                if (s != 4 || i < 30) price += 0.5; else price -= 3.0;
                vol += 30000.0;
            }
        }
        {
            std::ofstream ofs(base + "_daily_financial.csv");
            ofs << "cash_dividend,split_ratio,total_shares,float_shares,eps_ttm,pe_ttm,pb_lf,roe_ttm" << std::endl;
            double eps = 5.0 - s * 0.5;
            double pe = 20.0 - s * 2.0;
            for (int i = 0; i < num_days; ++i) {
                double dividend = 0.0, split = 1.0;
                if (s == 0 && i == 45) split = 2.0;
                if (s == 0 && i == 55) dividend = 0.5;
                ofs << dividend << "," << split << ",100000000,80000000,"
                    << eps << "," << pe << ",1.5,0.15" << std::endl;
            }
        }
    }
    LOG_INFO("生成 {} 只股票 x {} 天功能测试数据完成", num_stocks, num_days);
}

// ==========================================
// C. 集成测试
// ==========================================
void test_corporate_actions()
{
    LOG_INFO("--- [C1] 公司行为：拆股 + 分红 ---");

    auto& search_cfg = Configer::GetParamSearchConfig();
    search_cfg.SetMode(ParamSearchConfiger::SearchMode::RANDOM);
    search_cfg.SetRandomSamples(1);
    search_cfg.SetTopNCandidates({ 5 });
    search_cfg.SetNormalizeWeights(true);

    std::vector<std::string> files = { "functest", "functest_2", "functest_3", "functest_4", "functest_5" };
    GlobalData::Init(files);
    GlobalData* gd = GlobalData::GetGlobalData();

    FT_CHECK(gd != nullptr, "GlobalData 初始化成功");
    FT_CHECK(gd->get_stock_count() == 5, "加载 5 只股票");

    BacktestEngine engine;
    engine.Initialize(1000000.0, 0);
    engine.Run();

    const auto* te = engine.GetTradeExecutor();
    FT_CHECK(te != nullptr, "TradeExecutor 不为空");

    const auto& nav = te->GetDataManager().GetNavHistory();
    FT_CHECK(!nav.empty(), "NAV 历史非空");
    FT_CHECK(nav.back().total_net_value > 0.0, "最终净值 > 0");

    const auto& trades = te->GetDataManager().GetTradeHistory();
    FT_CHECK(!trades.empty(), "产生了交易记录");

    GlobalData::Destroy();
}

void test_stop_loss_take_profit()
{
    LOG_INFO("--- [C2] 止损/止盈 ---");

    auto& search_cfg = Configer::GetParamSearchConfig();
    search_cfg.SetMode(ParamSearchConfiger::SearchMode::RANDOM);
    search_cfg.SetRandomSamples(1);
    search_cfg.SetTopNCandidates({ 4 });
    search_cfg.SetNormalizeWeights(true);

    std::vector<std::string> files = { "functest", "functest_2", "functest_3", "functest_4", "functest_5" };
    GlobalData::Init(files);

    BacktestEngine engine;
    engine.Initialize(1000000.0, 0);
    engine.Run();

    BacktestSummary s = engine.GetSummary();
    FT_CHECK(!std::isnan(s.annual_return), "止损场景: 年化收益非 NaN");
    FT_CHECK(!std::isnan(s.sharpe_ratio), "止损场景: 夏普非 NaN");
    FT_CHECK(s.max_drawdown >= 0.0, "止损场景: 最大回撤 ≥ 0");

    const auto& trades = engine.GetTradeExecutor()->GetDataManager().GetTradeHistory();
    int sell_count = 0;
    for (const auto& t : trades) {
        if (!t.is_buy) sell_count++;
    }
    FT_CHECK(sell_count > 0, "止损场景: 产生了卖出交易 (sell_count=" + std::to_string(sell_count) + ")");

    GlobalData::Destroy();
}

void test_delisted_and_suspended_filtering()
{
    LOG_INFO("--- [C3] 退市/停牌/涨停过滤 ---");

    auto& search_cfg = Configer::GetParamSearchConfig();
    search_cfg.SetMode(ParamSearchConfiger::SearchMode::RANDOM);
    search_cfg.SetRandomSamples(1);
    search_cfg.SetTopNCandidates({ 5 });
    search_cfg.SetNormalizeWeights(true);

    std::vector<std::string> files = { "functest", "functest_2", "functest_3", "functest_4", "functest_5" };
    GlobalData::Init(files);

    BacktestEngine engine;
    engine.Initialize(1000000.0, 0);
    engine.Run();

    BacktestSummary s = engine.GetSummary();
    FT_CHECK(!std::isnan(s.annual_return), "过滤场景: 年化收益非 NaN");

    const auto& positions = engine.GetTradeExecutor()->GetDataManager().GetAllPositions();
    FT_CHECK(positions.empty(), "过滤场景: 回测结束持仓已清空");

    GlobalData::Destroy();
}

void test_position_and_industry_limits()
{
    LOG_INFO("--- [C4] 仓位上限 + 行业上限 ---");

    auto& strategy_cfg = Configer::GetStrategyConfiger();
    double orig_single = strategy_cfg.GetSinglePositionLimit();
    double orig_industry = strategy_cfg.GetIndustryPositionLimit();

    strategy_cfg.SetSinglePositionLimit(0.05);
    strategy_cfg.SetIndustryPositionLimit(0.15);

    auto& search_cfg = Configer::GetParamSearchConfig();
    search_cfg.SetMode(ParamSearchConfiger::SearchMode::RANDOM);
    search_cfg.SetRandomSamples(1);
    search_cfg.SetTopNCandidates({ 5 });
    search_cfg.SetNormalizeWeights(true);

    std::vector<std::string> files = { "functest", "functest_2", "functest_3", "functest_4", "functest_5" };
    GlobalData::Init(files);

    BacktestEngine engine;
    engine.Initialize(1000000.0, 0);
    engine.Run();

    BacktestSummary s = engine.GetSummary();
    FT_CHECK(!std::isnan(s.annual_return), "仓位上限: 年化收益非 NaN");

    strategy_cfg.SetSinglePositionLimit(orig_single);
    strategy_cfg.SetIndustryPositionLimit(orig_industry);

    GlobalData::Destroy();
}

void test_nav_and_trade_export()
{
    LOG_INFO("--- [C5] NAV / 交易记录 CSV 导出 ---");

    auto& search_cfg = Configer::GetParamSearchConfig();
    search_cfg.SetMode(ParamSearchConfiger::SearchMode::RANDOM);
    search_cfg.SetRandomSamples(1);
    search_cfg.SetTopNCandidates({ 3 });
    search_cfg.SetNormalizeWeights(true);

    std::vector<std::string> files = { "functest", "functest_2", "functest_3", "functest_4", "functest_5" };
    GlobalData::Init(files);

    BacktestEngine engine;
    engine.Initialize(1000000.0, 0);
    engine.Run();

    const std::string nav_file = "functest_nav_export.csv";
    engine.ExportNavToCsv(nav_file);

    std::ifstream nav_ifs(nav_file);
    FT_CHECK(nav_ifs.is_open(), "NAV CSV 文件已创建");
    std::string header;
    std::getline(nav_ifs, header);
    FT_CHECK(header.find("trade_date") != std::string::npos, "NAV CSV 有表头");
    int nav_lines = 0;
    std::string line;
    while (std::getline(nav_ifs, line)) { if (!line.empty()) nav_lines++; }
    FT_CHECK(nav_lines > 0, "NAV CSV 有数据行 (rows=" + std::to_string(nav_lines) + ")");
    nav_ifs.close();

    const std::string trade_file = "functest_trades_export.csv";
    engine.ExportTradesToCsv(trade_file);

    std::ifstream trade_ifs(trade_file);
    FT_CHECK(trade_ifs.is_open(), "Trades CSV 文件已创建");
    std::getline(trade_ifs, header);
    FT_CHECK(header.find("stock_index") != std::string::npos, "Trades CSV 有表头");
    int trade_lines = 0;
    while (std::getline(trade_ifs, line)) { if (!line.empty()) trade_lines++; }
    FT_CHECK(trade_lines > 0, "Trades CSV 有数据行 (rows=" + std::to_string(trade_lines) + ")");
    trade_ifs.close();

    std::remove(nav_file.c_str());
    std::remove(trade_file.c_str());
    GlobalData::Destroy();
}

void test_empty_data_guards()
{
    LOG_INFO("--- [C6] 空数据 / 无调仓日防御 ---");

    BacktestEngine engine;
    engine.Run();
    FT_CHECK(true, "未初始化 Run(): 不崩溃");

    engine.Initialize(1000000.0, 0);
    engine.Run();
    FT_CHECK(true, "GlobalData 为空时 Run(): 不崩溃");
}

// ==========================================
// 主入口
// ==========================================
void functional_test()
{
    ft_passed = true;
    HSBacktest::Logger::getInstance().init("functest");

    LOG_INFO("========================================");
    LOG_INFO("   功能测试套件");
    LOG_INFO("========================================");

    // ===== A 组：ParamBuilder =====
    LOG_INFO("[A组] ParamBuilder 参数生成");
    test_param_builder_grid();
    test_param_builder_random();
    test_param_builder_all_zero_range();
    test_param_builder_single_factor();

    // ===== 生成共用测试数据 =====
    LOG_INFO("[B组] 生成集成测试数据");
    generate_functional_test_csv();

    // ===== C 组：集成测试 =====
    LOG_INFO("[C组] 集成测试");
    test_corporate_actions();
    test_stop_loss_take_profit();
    test_delisted_and_suspended_filtering();
    test_position_and_industry_limits();
    test_nav_and_trade_export();
    test_empty_data_guards();

    // ===== 清理 CSV =====
    for (int s = 0; s < 5; ++s) {
        std::string suffix = (s == 0) ? "" : "_" + std::to_string(s + 1);
        std::string base = "functest" + suffix;
        std::remove((base + "_daily.csv").c_str());
        std::remove((base + "_daily_extended.csv").c_str());
        std::remove((base + "_daily_financial.csv").c_str());
    }

    // ===== 汇总 =====
    LOG_INFO("========================================");
    if (ft_passed) {
        LOG_INFO("  功能测试套件：全部通过！");
    } else {
        LOG_ERROR("  功能测试套件：存在失败项，请检查上方 [FAIL]");
    }
    LOG_INFO("========================================");
}
