#pragma once
#include<vector>

class log_mcap_data
{
private:
	std::vector<double> log_mcap_values; // 存储每只股票调仓日的对数市值值
public:
	double get_log_mcap(const int index) const
	{
		return log_mcap_values[index];
	}

	std::vector<double>& get_log_mcap_values()
	{
		return log_mcap_values;
	}

	log_mcap_data(int size) : log_mcap_values(size, 0.0)
	{}
};


class StockDailyData;
class StockDailyFinancialData;
class log_mcap
{
private:
    // 存储每只股票调仓日的对数市值值
    std::vector<double> &log_mcaps;

	inline double calculate_log_mcap(const std::vector<StockDailyData>& daily_datas, const std::vector<StockDailyFinancialData>& financial_datas, const int date_index) const;
public:
	void update_log_mcap(const std::vector<StockDailyData>& daily_datas, const std::vector<StockDailyFinancialData>& financial_datas, const std::vector<int>& rebalances_date_indexs);


	log_mcap(log_mcap_data& data) : log_mcaps(data.get_log_mcap_values())
	{}
};

