#include "log_mcap.h"
#include<cmath>
#include"../../data_defs.h"


inline double log_mcap::calculate_log_mcap(const std::vector<StockDailyData>& daily_datas, const std::vector<StockDailyFinancialData>& financial_datas, const int date_index) const
{
	double total_shares = financial_datas[date_index-1].total_shares;
	double close_price = daily_datas[date_index-1].close;
	return std::log(total_shares * close_price);
}

void log_mcap::update_log_mcap(const std::vector<StockDailyData>& daily_datas, const std::vector<StockDailyFinancialData>& financial_datas, const std::vector<int>& rebalances_date_indexs)
{
#pragma omp parallel for schedule(static) 
	for(int i = 0; i < rebalances_date_indexs.size(); ++i)
	{
		double log_mcap_value = calculate_log_mcap(daily_datas, financial_datas, rebalances_date_indexs[i]);
		log_mcaps[i] = log_mcap_value;
	}
}
