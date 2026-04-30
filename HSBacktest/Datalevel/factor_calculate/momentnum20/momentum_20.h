#pragma once
#include<vector>
class StockDailyData;
class momentum_20
{
private:
	//存储每只股票换手日的最近20个交易日动量值
	std::vector<double> momentum_20s; 
	/*
	* @brief 计算动量值
	* @param daily_datas 存储每只股票交易日的价格数据的deque
	* @param date_index 当前日期在deque中的索引
	*/
	double calculate_momentum_20(const std::vector<StockDailyData>& daily_datas,const int date_index) const;
public:
	void update_momentum_20(const std::vector<StockDailyData>& daily_datas, const std::vector<int>& rebalance_index);
	double get_momentum_20(const int index) const
	{
		return momentum_20s[index];
	}

	momentum_20(int size) :momentum_20s(size, 0.0)
	{}
};