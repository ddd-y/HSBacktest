#pragma once
#include"data_defs.h"
#include<string>
#include"factor_calculate/factorbase.h"

//计算动量因子时需要前20天的数据
constexpr const int PRE_EXTRA_DAYS = 20; 

class StockKData 
{
private:
	std::string stock_code;
	std::vector<StockDailyData> daily_datas; 
	std::vector<StockDailyExtendedData> extended_datas;
	std::vector<StockDailyFinancialData> financial_datas;
	std::vector<int> rebalance_index; //存储调仓日期在daily_datas中的索引
	FactorBase* factor_base;
public:
	StockKData(const std::string& code);

	inline void calculate_factors();

	//这一块结构不好，后边得改，先这样
	FactorBase* get_factor_base() const { return factor_base; }
};