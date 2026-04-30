#pragma once
#include"data_defs.h"
#include<string>

class FactorBase;
class StockKData 
{
private:
	std::string stock_code;
	std::vector<StockDailyData> daily_datas; 
	std::vector<StockDailyExtendedData> extended_datas;
	std::vector<StockDailyFinancialData> financial_datas;
	FactorBase* factor_base;
public:
	StockKData(const std::string& code);
};