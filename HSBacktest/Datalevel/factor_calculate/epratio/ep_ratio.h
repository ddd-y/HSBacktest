#pragma once
#include<vector>

class StockDailyData;
class StockDailyFinancialData;
class ep_ratio
{
private:
	//存储每只股票调仓日的盈利收益率(EP)值（EP = 1/PE_TTM）
	std::vector<double> ep_ratios;

	/*
	* @brief 计算盈利收益率(EP)值
	* @param daily_datas 存储每只股票交易日的价格数据的vector
	* @param financial_datas 存储每只股票交易日的财务数据的vector
	* @param date_index 当前日期在vector中的索引
	*/
	inline double calculate_ep_ratio(const std::vector<StockDailyData>& daily_datas, const std::vector<StockDailyFinancialData>& financial_datas, const int date_index) const;
public:
	/*
	* @brief 更新调仓日的盈利收益率(EP)值
	* @param daily_datas 存储每只股票交易日的价格数据的vector
	* @param financial_datas 存储每只股票交易日的财务数据的vector
	* @param rebalances_date_indexs 调仓日在vector中的索引列表
	*/
	void update_ep_ratio(const std::vector<StockDailyData>& daily_datas, const std::vector<StockDailyFinancialData>& financial_datas, const std::vector<int>& rebalances_date_indexs);

	/*
	* @brief 获取指定索引的盈利收益率(EP)值
	* @param index 调仓日索引
	* @return 对应调仓日的EP值
	*/
	double get_ep_ratio(const int index) const
	{
		return ep_ratios[index];
	}

	/*
	* @brief 构造函数
	* @param size 预分配的调仓日数量（用于vector内存预预留）
	*/
	ep_ratio(int size) :ep_ratios(size, 0.0)
	{}
};