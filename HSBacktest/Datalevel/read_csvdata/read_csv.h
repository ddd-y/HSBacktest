#pragma once
#include"../data_defs.h"
#include"csv.h"
#include"../../MyLog/Logger.h"
#include <vector>
#include <string>


namespace csvreader {
    // 读取StockDailyData
    inline std::vector<StockDailyData> read_stock_daily_data(const std::string& filename) {
        std::vector<StockDailyData> result;
        try {
            io::CSVReader<9> in(filename);
            in.read_header(io::ignore_extra_column,
                "trade_date", "close", "open", "adj_factor", "industry_code",
                "is_suspended", "is_delisted", "is_limit_up", "is_limit_down");
            StockDailyData row;
            while (in.read_row(row.trade_date, row.close, row.open, row.adj_factor, row.industry_code,
                row.is_suspended, row.is_delisted, row.is_limit_up, row.is_limit_down)) {
                result.emplace_back(row);
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR("读取StockDailyData CSV失败: {}", e.what());
        }
        return result;
    }


    inline std::vector<StockDailyExtendedData> read_stock_daily_extended_data(const std::string& filename)
    {
        std::vector<StockDailyExtendedData> result;
        try {
            io::CSVReader<4> in(filename);
            in.read_header(io::ignore_extra_column, "high", "low", "volume", "amount");
            StockDailyExtendedData row;
            while (in.read_row(row.high, row.low, row.volume, row.amount)) {
                result.emplace_back(row);
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR("读取StockDailyExtendedData CSV失败: {}", e.what());
        }
        return result;
    }

    // 读取StockDailyFinancialData
    inline std::vector<StockDailyFinancialData> read_stock_daily_financial_data(const std::string& filename)
    {
        std::vector<StockDailyFinancialData> result;
        try {
            // 字段数量：8个
            io::CSVReader<8> in(filename);
            in.read_header(io::ignore_extra_column,
                "cash_dividend", "split_ratio", "total_shares", "float_shares",
                "eps_ttm", "pe_ttm", "pb_lf", "roe_ttm");
            StockDailyFinancialData row;
            while (in.read_row(row.cash_dividend, row.split_ratio, row.total_shares, row.float_shares,
                row.eps_ttm, row.pe_ttm, row.pb_lf, row.roe_ttm)) {
                result.emplace_back(row);
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR("读取StockDailyFinancialData CSV失败: {}", e.what());
        }
        return result;
    }
}