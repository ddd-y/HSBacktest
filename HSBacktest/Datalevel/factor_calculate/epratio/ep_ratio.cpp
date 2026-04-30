#include "ep_ratio.h"
#include"../../data_defs.h"
#include<cmath>

//不做越界/除0判断（保持与现有momentum_20/log_mcap代码逻辑一致性）
inline double ep_ratio::calculate_ep_ratio(const std::vector<StockDailyData>& daily_datas, const std::vector<StockDailyFinancialData>& financial_datas, const int date_index) const
{
	//EP = 1 / PE_TTM，取调仓日前一日的财务数据（与log_mcap的date_index-1逻辑一致）
	double pe_ttm = financial_datas[date_index - 1].pe_ttm;
	//避免除0（可选补充：现有代码未做异常处理，若需严谨可加判断）
	if (std::fabs(pe_ttm) < 1e-9) {
		return 0.0;
	}
	return 1.0 / pe_ttm;
}

void ep_ratio::update_ep_ratio(const std::vector<StockDailyData>& daily_datas, const std::vector<StockDailyFinancialData>& financial_datas, const std::vector<int>& rebalances_date_indexs)
{
#pragma omp parallel for schedule(static) 
	for (int i = 0; i < rebalances_date_indexs.size(); ++i)
	{
		double ep_ratio_value = calculate_ep_ratio(daily_datas, financial_datas, rebalances_date_indexs[i]);
		ep_ratios[i] = ep_ratio_value;
	}
}