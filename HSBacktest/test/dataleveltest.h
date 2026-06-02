#pragma once

#include "../HSBacktest.h"
#include"../MyLog/Logger.h"
#include "../Datalevel/read_csvdata/read_csv.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>
#include "../Datalevel/data_defs.h"
#include "../Datalevel/factor_calculate/momentnum20/momentum_20.h"
#include "../Datalevel/factor_calculate/volatility20/volatility_20.h"
#include "../Datalevel/factor_calculate/epratio/ep_ratio.h"
#include "../Datalevel/factor_calculate/logmcap/log_mcap.h"
#include "../Datalevel/factor_calculate/turnover20/turnover_20.h"
#include "../Datalevel/Global_data.h"

// 生成22个交易日的测试数据
void generate_test_data(std::vector<StockDailyData>& daily_datas,
    std::vector<StockDailyExtendedData>& extended_datas,
    std::vector<StockDailyFinancialData>& financial_datas) {

    // 生成22个交易日的数据（索引0-21）
    double price = 100.0;
    double volume = 1000000.0; // 100万股

    for (int i = 0; i < 22; ++i) {
        StockDailyData daily;
        daily.trade_date = 20250101 + i;
        daily.close = price;
        daily.open = price - 0.5;
        daily.adj_factor = 1.0;
        daily.industry_code = (i % 5 == 0) ? 1 : 2;  // 两个行业交替
        daily.is_suspended = 0;
        daily.is_delisted = 0;
        daily.is_limit_up = 0;
        daily.is_limit_down = 0;
        daily_datas.push_back(daily);

        // 价格简单增长
        price += 1.0;

        StockDailyExtendedData ext;
        ext.high = price + 0.5;
        ext.low = price - 0.5;
        ext.volume = volume;
        ext.amount = volume * price;
        extended_datas.push_back(ext);

        // 成交量稍微变化
        volume += 50000.0;

        StockDailyFinancialData financial;
        financial.cash_dividend = 0.0;       // 现金分红
        financial.split_ratio = 1.0;         // 拆股比例
        financial.total_shares = 100000000; // 1亿股
        financial.float_shares = 80000000;  // 8000万股
        financial.eps_ttm = 5.0;            // 每股收益
        financial.pe_ttm = 20.0;            // PE为20
        financial.pb_lf = 1.5;              // 市净率
        financial.roe_ttm = 0.15;           // ROE
        financial_datas.push_back(financial);
    }
}

// 手工计算预期值用于验证
void calculate_expected_values(const std::vector<StockDailyData>& daily_datas,
    const std::vector<StockDailyFinancialData>& financial_datas,
    const std::vector<StockDailyExtendedData>& extended_datas) {

    std::cout << std::endl << "=== 手工计算预期值（修正版） ===" << std::endl;

    // 测试第一个调仓日（date_index = 21，因为PRE_EXTRA_DAYS = 20）
    int date_index = 21; // 第21天（调仓日索引）
    std::cout << "测试调仓日索引: " << date_index << std::endl;
    std::cout << std::fixed << std::setprecision(6);

    // 1. 动量因子（20日）：(最新价/20天前价) - 1 （对齐实际因子计算逻辑）
    // 范围：date_index-21（20天前） 到 date_index-1（调仓日前一天）
    if (date_index >= 21) {
        int start_idx = date_index - 21; // 0（20天前）
        int end_idx = date_index - 1;    // 20（调仓日前一天）

        // 必须叠加复权因子，和GlobalData内部计算对齐
        double start_price = daily_datas[start_idx].close * daily_datas[start_idx].adj_factor;
        double end_price = daily_datas[end_idx].close * daily_datas[end_idx].adj_factor;

        double momentum_manual = (end_price / start_price) - 1.0; // 正确的收益率公式
        std::cout << "1. 动量因子（20日）: " << momentum_manual
            << " (第" << start_idx << "天复权价=" << start_price
            << " | 第" << end_idx << "天复权价=" << end_price << ")" << std::endl;
    }

    // 2. EP比率因子：1/PE_TTM（调仓日前一天的PE_TTM）
    int pe_date_idx = date_index - 1; // 20
    double pe_ttm = financial_datas[pe_date_idx].pe_ttm;
    double ep_ratio_manual = 1.0 / pe_ttm;
    std::cout << "2. EP比率因子: " << ep_ratio_manual
        << " (1 / 第" << pe_date_idx << "天PE_TTM=" << pe_ttm << ")" << std::endl;

    // 3. 对数市值因子：ln(总股数 * 调仓日前一天收盘价)
    int mcap_date_idx = date_index - 1; // 20
    double total_shares = financial_datas[mcap_date_idx].total_shares;
    double close_price = daily_datas[mcap_date_idx].close * daily_datas[mcap_date_idx].adj_factor; // 复权价
    double mcap = total_shares * close_price;
    double log_mcap_manual = std::log(mcap);
    std::cout << "3. 对数市值因子: " << log_mcap_manual
        << " (ln(" << total_shares << " * " << close_price << ") = ln(" << mcap << "))" << std::endl;

    // 4. 换手率因子（20日累计）：过去20天每日换手率（成交量/流通股本）之和
    double total_turnover = 0.0;
    int turnover_start = date_index - 21; // 0
    int turnover_end = date_index - 1;   // 20
    std::cout << "4. 换手率因子计算范围：第" << turnover_start << "天 到 第" << turnover_end - 1 << "天（共20天）" << std::endl;
    for (int i = turnover_start; i < turnover_end; ++i) { // 0~19，共20天
        double vol = extended_datas[i].volume;
        double float_shares = financial_datas[i].float_shares;
        if (float_shares > 0) {
            double daily_turnover = vol / float_shares;
            total_turnover += daily_turnover;
            // 可选：打印每日换手率（调试用）
            // std::cout << "  第" << i << "天换手率: " << daily_turnover << std::endl;
        }
    }
    std::cout << "   20日累计换手率因子: " << total_turnover << std::endl;

    // 5. 波动率因子（20日收益率标准差）：过去20天日收益率的无偏标准差
    if (date_index >= 21) {
        std::vector<double> returns;
        int vol_start_idx = date_index - 21; // 0（20天前）
        int vol_end_idx = date_index - 1;    // 20（调仓日前一天）

        // 计算20个日收益率（需要21个价格点：0~20 → 生成20个收益率）
        double prev_price = daily_datas[vol_start_idx].close * daily_datas[vol_start_idx].adj_factor;
        for (int i = vol_start_idx + 1; i <= vol_end_idx; ++i) { // 1~20，共20个收益率
            double curr_price = daily_datas[i].close * daily_datas[i].adj_factor;
            double daily_ret = std::log(curr_price / prev_price); // 对数收益率
            returns.push_back(daily_ret);
            prev_price = curr_price;
        }

        // 计算均值
        double sum_ret = 0.0;
        for (double ret : returns) sum_ret += ret;
        double mean_ret = sum_ret / returns.size();

        // 计算无偏方差（除以n-1）
        double variance = 0.0;
        for (double ret : returns) {
            variance += std::pow(ret - mean_ret, 2);
        }
        double stddev = std::sqrt(variance / (returns.size() - 1)); // 无偏标准差

        std::cout << "5. 波动率因子（20日）: " << stddev
            << " (基于" << returns.size() << "个对数收益率，均值=" << mean_ret << ")" << std::endl;
    }
}

// 生成包含22个交易日的测试CSV文件（所有三个文件）
void generate_test_csv_file() {
    // 1. 主日线数据文件
    std::string daily_filename = "test_22days_daily.csv";
    std::ofstream daily_ofs(daily_filename);

    if (!daily_ofs.is_open()) {
        std::cerr << "无法创建测试文件: " << daily_filename << std::endl;
        return;
    }

    // 写入CSV头部
    daily_ofs << "trade_date,close,open,adj_factor,industry_code,is_suspended,is_delisted,is_limit_up,is_limit_down" << std::endl;

    // 生成22个交易日的数据
    double price = 100.0;
    double volume = 1000000.0; // 100万股

    for (int i = 0; i < 22; ++i) {
        int trade_date = 20250101 + i;
        double close_price = price;
        double open_price = price - 0.5;

        daily_ofs << trade_date << ","
            << std::fixed << std::setprecision(2) << close_price << ","
            << open_price << ","
            << "1.0,0,0,0,0,0" << std::endl;  // adj_factor, industry_code, is_suspended, is_delisted, is_limit_up, is_limit_down

        price += 1.0;
    }

    daily_ofs.close();
    std::cout << "已生成测试日线数据文件: " << daily_filename << " 包含22个交易日数据" << std::endl;

    // 2. 扩展数据文件
    std::string ext_filename = "test_22days_daily_extended.csv";
    std::ofstream ext_ofs(ext_filename);

    if (!ext_ofs.is_open()) {
        std::cerr << "无法创建扩展数据文件: " << ext_filename << std::endl;
        return;
    }

    ext_ofs << "trade_date,high,low,volume,amount" << std::endl;

    price = 100.0;
    volume = 1000000.0;

    for (int i = 0; i < 22; ++i) {
        int trade_date = 20250101 + i;
        double high_price = price + 0.5;
        double low_price = price - 0.5;
        double amount = volume * price;

        ext_ofs << trade_date << ","
            << std::fixed << std::setprecision(2) << high_price << ","
            << low_price << ","
            << volume << ","
            << amount << std::endl;

        price += 1.0;
        volume += 50000.0;
    }

    ext_ofs.close();
    std::cout << "已生成测试扩展数据文件: " << ext_filename << std::endl;

    // 3. 财务数据文件
    std::string fin_filename = "test_22days_daily_financial.csv";
    std::ofstream fin_ofs(fin_filename);

    if (!fin_ofs.is_open()) {
        std::cerr << "无法创建财务数据文件: " << fin_filename << std::endl;
        return;
    }

    // 写入CSV头部，字段顺序必须与read_csv.h中的定义完全一致
    fin_ofs << "cash_dividend,split_ratio,total_shares,float_shares,eps_ttm,pe_ttm,pb_lf,roe_ttm" << std::endl;

    for (int i = 0; i < 22; ++i) {
        double cash_dividend = 0.0;       // 现金分红
        double split_ratio = 1.0;         // 拆股比例
        int64_t total_shares = 100000000; // 1亿股
        int64_t float_shares = 80000000;  // 8000万股
        double eps_ttm = 5.0;             // 每股收益
        double pe_ttm = 20.0;             // PE比率
        double pb_lf = 1.5;               // 市净率
        double roe_ttm = 0.15;            // ROE

        fin_ofs << cash_dividend << ","
            << split_ratio << ","
            << total_shares << ","
            << float_shares << ","
            << eps_ttm << ","
            << pe_ttm << ","
            << pb_lf << ","
            << roe_ttm << std::endl;
    }

    fin_ofs.close();
    std::cout << "已生成测试财务数据文件: " << fin_filename << std::endl;
}

void dataleveltest()
{
    HSBacktest::Logger::getInstance().init("mylog");

    std::cout << "=== 因子计算逻辑验证 ===" << std::endl;

    // 1. 创建测试CSV文件
    generate_test_csv_file();

    // 2. 使用GlobalData加载数据
    // StockKData会自动添加后缀，所以只需要传递基础名称
    std::vector<std::string> stock_files = { "test_22days" };

    std::cout << "初始化GlobalData..." << std::endl;
    GlobalData::Init(stock_files);

    GlobalData& global_data = *GlobalData::GetGlobalData();
    std::cout << "加载了 " << global_data.get_stock_count() << " 只股票" << std::endl;

    if (global_data.get_stock_count() == 0) {
        std::cerr << "错误：没有加载到股票数据！" << std::endl;
    }

    // 3. 获取第一个股票的数据进行验证
    StockKData* stock_data = global_data.get_stock_k_data(0);
    FactorDatabase* factor_db = global_data.get_factor_database(0);

    if (!stock_data || !factor_db) {
        std::cerr << "错误：无法获取股票数据或因子数据库！" << std::endl;
    }

    // 4. 验证基本数据
    const auto& daily_datas = stock_data->get_daily_datas();
    const auto& rebalance_index = global_data.get_rebalance_index();

    std::cout << "股票数据天数: " << daily_datas.size() << std::endl;
    std::cout << "调仓日数量: " << rebalance_index.size() << std::endl;

    if (!rebalance_index.empty()) {
        std::cout << "第一个调仓日索引: " << rebalance_index[0] << std::endl;
    }

    // 5. 手工计算预期值用于对比（使用实际加载的数据）
    calculate_expected_values(daily_datas,
        stock_data->get_financial_datas(),
        stock_data->get_extended_datas());

    std::cout << std::endl << "=== 使用GlobalData的因子计算结果 ===" << std::endl;

    // 6. 获取因子计算结果
    // 注意：这里假设因子已经通过GlobalData的构造函数计算完成
    // GlobalData内部会调用因子计算
    std::cout << "ep_ratio结果对比: global计算值：" << global_data.get_factor_database(0)->get_ep_ratio_data().get_ep_ratio_values()[0]<<std::endl;
    std::cout << "momentum_20结果对比: global计算值：" << global_data.get_factor_database(0)->get_momentum_20_data().get_momentum_20_values()[0] << std::endl;
    std::cout << "log_mcap结果对比: global计算值：" << global_data.get_factor_database(0)->get_log_mcap_data().get_log_mcap_values()[0] << std::endl;
    std::cout << "volatility_20结果对比: global计算值：" << global_data.get_factor_database(0)->get_volatility_20_data().get_volatility_20_values()[0] << std::endl;
    std::cout << "turnover_20结果对比: global计算值：" << global_data.get_factor_database(0)->get_turnover_20_data().get_turnover_20_values()[0] << std::endl;

    std::cout << "因子计算已完成（通过GlobalData构造函数）" << std::endl;

    std::cout << std::endl << "=== 测试完成 ===" << std::endl;
}
