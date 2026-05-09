#pragma once
#include"data_defs.h"
#include<string>

//计算动量因子时需要前21天的数据
constexpr const int PRE_EXTRA_DAYS = 21;

class StockKData
{
private:
	std::string stock_code;
	std::vector<StockDailyData> daily_datas;
	std::vector<StockDailyExtendedData> extended_datas;
	std::vector<StockDailyFinancialData> financial_datas;
	std::vector<int> rebalance_index;
public:
	StockKData(const std::string& code);

	int get_rebalance_count() const { return static_cast<int>(rebalance_index.size()); }

	const std::vector<StockDailyData>& get_daily_datas() const { return daily_datas; }
	const std::vector<StockDailyExtendedData>& get_extended_datas() const { return extended_datas; }
	const std::vector<StockDailyFinancialData>& get_financial_datas() const { return financial_datas; }
	const std::vector<int>& get_rebalance_index() const { return rebalance_index; }
};