#pragma once
#include<vector>

class volatility_20_data
{
private:
	std::vector<double> volatility_20_values; // 存储每只股票调仓日的最近20个交易日波动率值
public:
	double get_volatility_20(const int index) const
	{
		return volatility_20_values[index];
	}

	std::vector<double>& get_volatility_20_values()
	{
		return volatility_20_values;
	}

	volatility_20_data(int size) : volatility_20_values(size, 0.0)
	{}
};

class StockDailyData;
class volatility_20
{
private:
	//存储每只股票调仓日的最近20个交易日波动率值
	std::vector<double> &volatility_20s;

	/*
	* @brief 计算20日波动率值（收益率标准差）
	* @param daily_datas 存储每只股票交易日的价格数据的vector
	* @param date_index 当前日期在vector中的索引
	* @return 20日收益率标准差（波动率）
	*/
	inline double calculate_volatility_20(const std::vector<StockDailyData>& daily_datas, const int date_index) const;

public:
	/*
	* @brief 更新调仓日的20日波动率值
	* @param daily_datas 存储每只股票交易日的价格数据的vector
	* @param rebalance_index 调仓日在vector中的索引列表
	*/
	void update_volatility_20(const std::vector<StockDailyData>& daily_datas, const std::vector<int>& rebalance_index);

	/*
	* @brief 构造函数
	* @param size 预分配的调仓日数量（用于vector内存预预留）
	*/
	volatility_20(volatility_20_data& data) : volatility_20s(data.get_volatility_20_values())
	{}
};

