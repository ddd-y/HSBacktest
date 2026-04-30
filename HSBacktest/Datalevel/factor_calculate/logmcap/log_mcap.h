#pragma once
#include<vector>

class StockDailyData;
class StockDailyFinancialData;
class log_mcap
{
private:
    // 存储每只股票调仓日的对数市值值
    std::vector<double> log_mcaps;

	inline double calculate_log_mcap(const std::vector<StockDailyData>& daily_datas, const std::vector<StockDailyFinancialData>& financial_datas, const int date_index) const;
public:
	void update_log_mcap(const std::vector<StockDailyData>& daily_datas, const std::vector<StockDailyFinancialData>& financial_datas, const std::vector<int>& rebalances_date_indexs);

	double get_log_mcap(const int index) const
	{
		return log_mcaps[index];
	}

	log_mcap(int size) :log_mcaps(size, 0.0)
	{}
};