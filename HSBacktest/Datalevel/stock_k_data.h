#pragma once
#include"data_defs.h"
#include<string>



class StockKData
{
private:
	std::string stock_code;
	std::vector<StockDailyData> daily_datas;
	std::vector<StockDailyExtendedData> extended_datas;
	std::vector<StockDailyFinancialData> financial_datas;
public:
	StockKData(const std::string& code);


	const std::vector<StockDailyData>& get_daily_datas() const { return daily_datas; }
	const std::vector<StockDailyExtendedData>& get_extended_datas() const { return extended_datas; }
	const std::vector<StockDailyFinancialData>& get_financial_datas() const { return financial_datas; }
};