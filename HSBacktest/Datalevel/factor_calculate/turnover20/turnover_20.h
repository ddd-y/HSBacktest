#pragma once
#include<vector>

class StockDailyData;
class StockDailyExtendedData;
class StockDailyFinancialData;
class turnover_20
{
private:
	//存储每只股票调仓日的最近20个交易日换手率累计值
	std::vector<double> turnover_20s;
	/*
	* @brief 计算20日换手率值
	* @param extended_datas 存储每只股票交易日的扩展行情数据的vector
	* @param financial_datas 存储每只股票交易日的财务数据的vector
	* @param date_index 当前日期在vector中的索引
	*/
	inline double calculate_turnover_20(const std::vector<StockDailyExtendedData>& extended_datas, const std::vector<StockDailyFinancialData>& financial_datas, const int date_index) const;
public:
	/*
	* @brief 更新调仓日的20日换手率值
	* @param extended_datas 存储每只股票交易日的扩展行情数据的vector
	* @param financial_datas 存储每只股票交易日的财务数据的vector
	* @param rebalance_index 调仓日在vector中的索引列表
	*/
	void update_turnover_20(const std::vector<StockDailyExtendedData>& extended_datas, const std::vector<StockDailyFinancialData>& financial_datas, const std::vector<int>& rebalance_index);

	/*
	* @brief 获取指定索引的20日换手率值
	* @param index 调仓日索引
	* @return 对应调仓日的20日换手率值
	*/
	double get_turnover_20(const int index) const
	{
		return turnover_20s[index];
	}

	/*
	* @brief 构造函数
	* @param size 预分配的调仓日数量（用于vector内存预预留）
	*/
	turnover_20(int size) : turnover_20s(size, 0.0)
	{}
};