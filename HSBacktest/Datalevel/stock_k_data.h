#pragma once
#include"data_defs.h"
#include<string>



class StockKData
{
private:
	std::string stock_code;
	int32_t industry_code = 0;           // 行业代码（从每日数据中取最新有效值）
	std::vector<StockDailyData> daily_datas;
	std::vector<StockDailyExtendedData> extended_datas;
	std::vector<StockDailyFinancialData> financial_datas;

	// 从 daily_datas 中提取行业代码（取第一个非0值）
	inline void resolve_industry_code();
public:
	StockKData(const std::string& code);

	// 获取行业代码
	int32_t GetIndustryCode() const { return industry_code; }

	const std::vector<StockDailyData>& get_daily_datas() const { return daily_datas; }
	const std::vector<StockDailyExtendedData>& get_extended_datas() const { return extended_datas; }
	const std::vector<StockDailyFinancialData>& get_financial_datas() const { return financial_datas; }
};