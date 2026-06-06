#pragma once

#include "../HSBacktest.h"
#include"../MyLog/Logger.h"
#include "../Datalevel/read_csvdata/read_csv.h"
#include <fstream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <cstdio>
#include "../Datalevel/data_defs.h"
#include "../Datalevel/factor_calculate/momentnum20/momentum_20.h"
#include "../Datalevel/factor_calculate/volatility20/volatility_20.h"
#include "../Datalevel/factor_calculate/epratio/ep_ratio.h"
#include "../Datalevel/factor_calculate/logmcap/log_mcap.h"
#include "../Datalevel/factor_calculate/turnover20/turnover_20.h"
#include "../Datalevel/Global_data.h"

// 断言宏
#define DL_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            LOG_ERROR("[FAIL] {} ({}:{})", msg, __FILE__, __LINE__); \
            dl_passed = false; \
        } else { \
            LOG_INFO("[PASS] {}", msg); \
        } \
    } while(0)

static bool dl_passed = true;

// 生成22个交易日的测试数据
void generate_test_data(std::vector<StockDailyData>& daily_datas,
    std::vector<StockDailyExtendedData>& extended_datas,
    std::vector<StockDailyFinancialData>& financial_datas) {

    double price = 100.0;
    double volume = 1000000.0;

    for (int i = 0; i < 22; ++i) {
        StockDailyData daily;
        daily.trade_date = 20250101 + i;
        daily.close = price;
        daily.open = price - 0.5;
        daily.adj_factor = 1.0;
        daily.industry_code = (i % 5 == 0) ? 1 : 2;
        daily.is_suspended = 0;
        daily.is_delisted = 0;
        daily.is_limit_up = 0;
        daily.is_limit_down = 0;
        daily_datas.push_back(daily);

        price += 1.0;

        StockDailyExtendedData ext;
        ext.high = price + 0.5;
        ext.low = price - 0.5;
        ext.volume = volume;
        ext.amount = volume * price;
        extended_datas.push_back(ext);

        volume += 50000.0;

        StockDailyFinancialData financial;
        financial.cash_dividend = 0.0;
        financial.split_ratio = 1.0;
        financial.total_shares = 100000000;
        financial.float_shares = 80000000;
        financial.eps_ttm = 5.0;
        financial.pe_ttm = 20.0;
        financial.pb_lf = 1.5;
        financial.roe_ttm = 0.15;
        financial_datas.push_back(financial);
    }
}

// 手工计算预期值用于验证
void calculate_expected_values(const std::vector<StockDailyData>& daily_datas,
    const std::vector<StockDailyFinancialData>& financial_datas,
    const std::vector<StockDailyExtendedData>& extended_datas) {

    LOG_INFO("=== 手工计算预期值 ===");

    int date_index = 21;
    LOG_INFO("测试调仓日索引: {}", date_index);

    // 1. 动量因子（20日）
    if (date_index >= 21) {
        int start_idx = date_index - 21;
        int end_idx = date_index - 1;
        double start_price = daily_datas[start_idx].close * daily_datas[start_idx].adj_factor;
        double end_price = daily_datas[end_idx].close * daily_datas[end_idx].adj_factor;
        double momentum_manual = (end_price / start_price) - 1.0;
        LOG_INFO("1. 动量因子（20日）: {:.6f} (第{}天复权价={:.2f} | 第{}天复权价={:.2f})",
            momentum_manual, start_idx, start_price, end_idx, end_price);
    }

    // 2. EP比率因子
    int pe_date_idx = date_index - 1;
    double pe_ttm = financial_datas[pe_date_idx].pe_ttm;
    double ep_ratio_manual = 1.0 / pe_ttm;
    LOG_INFO("2. EP比率因子: {:.6f} (1 / 第{}天PE_TTM={})", ep_ratio_manual, pe_date_idx, pe_ttm);

    // 3. 对数市值因子
    int mcap_date_idx = date_index - 1;
    double total_shares = financial_datas[mcap_date_idx].total_shares;
    double close_price = daily_datas[mcap_date_idx].close * daily_datas[mcap_date_idx].adj_factor;
    double mcap = total_shares * close_price;
    double log_mcap_manual = std::log(mcap);
    LOG_INFO("3. 对数市值因子: {:.6f} (ln({} * {:.2f}) = ln({:.0f}))",
        log_mcap_manual, total_shares, close_price, mcap);

    // 4. 换手率因子（20日累计）
    double total_turnover = 0.0;
    int turnover_start = date_index - 21;
    int turnover_end = date_index - 1;
    for (int i = turnover_start; i < turnover_end; ++i) {
        double vol = extended_datas[i].volume;
        double float_shares = financial_datas[i].float_shares;
        if (float_shares > 0) {
            total_turnover += vol / float_shares;
        }
    }
    LOG_INFO("4. 20日累计换手率因子: {:.6f} (第{}天~第{}天)", total_turnover, turnover_start, turnover_end - 1);

    // 5. 波动率因子（20日收益率标准差）
    if (date_index >= 21) {
        std::vector<double> returns;
        int vol_start_idx = date_index - 21;
        int vol_end_idx = date_index - 1;
        double prev_price = daily_datas[vol_start_idx].close * daily_datas[vol_start_idx].adj_factor;
        for (int i = vol_start_idx + 1; i <= vol_end_idx; ++i) {
            double curr_price = daily_datas[i].close * daily_datas[i].adj_factor;
            returns.push_back(std::log(curr_price / prev_price));
            prev_price = curr_price;
        }
        double sum_ret = 0.0;
        for (double ret : returns) sum_ret += ret;
        double mean_ret = sum_ret / returns.size();
        double variance = 0.0;
        for (double ret : returns) variance += std::pow(ret - mean_ret, 2);
        double stddev = std::sqrt(variance / (returns.size() - 1));
        LOG_INFO("5. 波动率因子（20日）: {:.6f} (基于{}个对数收益率，均值={:.6f})",
            stddev, returns.size(), mean_ret);
    }
}

// 生成包含22个交易日的测试CSV文件
void generate_test_csv_file() {
    // 1. 主日线数据文件
    std::string daily_filename = "test_22days_daily.csv";
    std::ofstream daily_ofs(daily_filename);
    if (!daily_ofs.is_open()) {
        LOG_ERROR("无法创建测试文件: {}", daily_filename);
        return;
    }
    daily_ofs << "trade_date,close,open,adj_factor,industry_code,is_suspended,is_delisted,is_limit_up,is_limit_down" << std::endl;
    double price = 100.0;
    for (int i = 0; i < 22; ++i) {
        daily_ofs << (20250101 + i) << ","
            << std::fixed << std::setprecision(2) << price << ","
            << (price - 0.5) << ",1.0,0,0,0,0,0" << std::endl;
        price += 1.0;
    }
    daily_ofs.close();
    LOG_INFO("已生成测试日线数据文件: {} (22天)", daily_filename);

    // 2. 扩展数据文件
    std::string ext_filename = "test_22days_daily_extended.csv";
    std::ofstream ext_ofs(ext_filename);
    if (!ext_ofs.is_open()) {
        LOG_ERROR("无法创建扩展数据文件: {}", ext_filename);
        return;
    }
    ext_ofs << "trade_date,high,low,volume,amount" << std::endl;
    price = 100.0;
    double volume = 1000000.0;
    for (int i = 0; i < 22; ++i) {
        ext_ofs << (20250101 + i) << ","
            << std::fixed << std::setprecision(2) << (price + 0.5) << ","
            << (price - 0.5) << "," << volume << "," << (volume * price) << std::endl;
        price += 1.0;
        volume += 50000.0;
    }
    ext_ofs.close();
    LOG_INFO("已生成测试扩展数据文件: {}", ext_filename);

    // 3. 财务数据文件
    std::string fin_filename = "test_22days_daily_financial.csv";
    std::ofstream fin_ofs(fin_filename);
    if (!fin_ofs.is_open()) {
        LOG_ERROR("无法创建财务数据文件: {}", fin_filename);
        return;
    }
    fin_ofs << "cash_dividend,split_ratio,total_shares,float_shares,eps_ttm,pe_ttm,pb_lf,roe_ttm" << std::endl;
    for (int i = 0; i < 22; ++i) {
        fin_ofs << "0.0,1.0,100000000,80000000,5.0,20.0,1.5,0.15" << std::endl;
    }
    fin_ofs.close();
    LOG_INFO("已生成测试财务数据文件: {}", fin_filename);
}

void dataleveltest()
{
    dl_passed = true;
    HSBacktest::Logger::getInstance().init("mylog");

    LOG_INFO("========================================");
    LOG_INFO("   因子计算逻辑验证");
    LOG_INFO("========================================");

    // 1. 创建测试CSV文件
    LOG_INFO("[1/5] 生成测试CSV...");
    generate_test_csv_file();

    // 2. 使用GlobalData加载数据
    LOG_INFO("[2/5] 初始化GlobalData...");
    std::vector<std::string> stock_files = { "test_22days" };
    GlobalData::Init(stock_files);

    GlobalData& global_data = *GlobalData::GetGlobalData();
    DL_CHECK(global_data.get_stock_count() == 1, "股票数 == 1");

    StockKData* stock_data = global_data.get_stock_k_data(0);
    FactorDatabase* factor_db = global_data.get_factor_database(0);
    DL_CHECK(stock_data != nullptr, "StockKData 不为空");
    DL_CHECK(factor_db != nullptr, "FactorDatabase 不为空");

    const auto& daily_datas = stock_data->get_daily_datas();
    const auto& rebalance_index = global_data.get_rebalance_index();
    DL_CHECK(daily_datas.size() == 22, "日线数据天数 == 22");
    DL_CHECK(!rebalance_index.empty(), "调仓日列表非空");

    LOG_INFO("  (天数={}, 调仓日={})", daily_datas.size(), rebalance_index.size());

    // 3. 手工计算预期值
    LOG_INFO("[3/5] 手工计算预期值...");
    calculate_expected_values(daily_datas,
        stock_data->get_financial_datas(),
        stock_data->get_extended_datas());

    // 4. 对比 GlobalData 因子计算结果
    LOG_INFO("[4/5] 对比因子计算结果...");

    if (!rebalance_index.empty()) {
        int rb_idx = rebalance_index[0];

        double mom  = factor_db->get_momentum_20_data().get_momentum_20(rb_idx);
        double vol  = factor_db->get_volatility_20_data().get_volatility_20(rb_idx);
        double ep   = factor_db->get_ep_ratio_data().get_ep_ratio(rb_idx);
        double mcap = factor_db->get_log_mcap_data().get_log_mcap(rb_idx);
        double to   = factor_db->get_turnover_20_data().get_turnover_20(rb_idx);

        DL_CHECK(!std::isnan(mom) && !std::isinf(mom), "动量因子非 NaN/Inf");
        DL_CHECK(!std::isnan(vol) && !std::isinf(vol), "波动率因子非 NaN/Inf");
        DL_CHECK(!std::isnan(ep)  && !std::isinf(ep),  "EP因子非 NaN/Inf");
        DL_CHECK(!std::isnan(mcap)&& !std::isinf(mcap), "对数市值因子非 NaN/Inf");
        DL_CHECK(!std::isnan(to)  && !std::isinf(to),   "换手率因子非 NaN/Inf");

        // EP = 1/PE_TTM = 1/20 = 0.05
        DL_CHECK(std::abs(ep - 0.05) < 1e-6, "EP因子 ≈ 0.05 (1/20)");

        LOG_INFO("  momentum={:.6f}", mom);
        LOG_INFO("  volatility={:.6f}", vol);
        LOG_INFO("  ep_ratio={:.6f}", ep);
        LOG_INFO("  log_mcap={:.6f}", mcap);
        LOG_INFO("  turnover={:.6f}", to);
    }

    // 5. 清理
    LOG_INFO("[5/5] 清理...");
    GlobalData::Destroy();
    std::remove("test_22days_daily.csv");
    std::remove("test_22days_daily_extended.csv");
    std::remove("test_22days_daily_financial.csv");

    // 汇总
    LOG_INFO("========================================");
    if (dl_passed) {
        LOG_INFO("  因子计算逻辑验证：全部通过！");
    } else {
        LOG_ERROR("  因子计算逻辑验证：存在失败项");
    }
    LOG_INFO("========================================");
}
