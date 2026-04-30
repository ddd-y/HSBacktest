#include "turnover_20.h"
#include"../../data_defs.h"


inline double turnover_20::calculate_turnover_20(const std::vector<StockDailyExtendedData>& extended_datas, const std::vector<StockDailyFinancialData>& financial_datas, const int date_index) const
{
	double total_turnover = 0.0;
	// 时间窗口与momentum_20完全对齐：date_index-21 到 date_index-1 共20个交易日
	for (int i = date_index - 21; i < date_index - 1; ++i)
	{
		// 换手率 = 当日成交量 / 流通股本（单位均为“股”，无需额外转换）
		int64_t float_shares = financial_datas[i].float_shares;
		// 避免除0（可选补充，现有代码库未强制异常处理）
		if (float_shares == 0) {
			continue;
		}
		double volume = extended_datas[i].volume;
		total_turnover += (volume / float_shares);
	}
	return total_turnover;
}

void turnover_20::update_turnover_20(const std::vector<StockDailyExtendedData>& extended_datas, const std::vector<StockDailyFinancialData>& financial_datas, const std::vector<int>& rebalance_index)
{
#pragma omp parallel for schedule(static) 
	for (int i = 0; i < rebalance_index.size(); ++i)
	{
		double turnover_20_value = calculate_turnover_20(extended_datas, financial_datas, rebalance_index[i]);
		turnover_20s[i] = turnover_20_value;
	}
}