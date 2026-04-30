#include "momentum_20.h"
#include"../../data_defs.h"
#include<array>

//不做越界判断
double momentum_20::calculate_momentum_20(const std::vector<StockDailyData>& daily_datas, const int date_index) const
{
	int start_index = date_index - 21;
	double start_price = daily_datas[start_index].close * daily_datas[start_index].adj_factor;
	double end_price = daily_datas[date_index - 1].close * daily_datas[date_index - 1].adj_factor;
	return (end_price - start_price) / start_price;
}

void momentum_20::update_momentum_20(const std::vector<StockDailyData>& daily_datas, const std::vector<int>& rebalance_index)
{
#pragma omp parallel for schedule(static) 
	for (int i = 0; i < rebalance_index.size(); ++i)
	{
		double momentum_20_value = calculate_momentum_20(daily_datas, rebalance_index[i]);
		momentum_20s[i] = momentum_20_value;
	}
}
