#pragma once
/*
struct alignas(CACHE_LINE_SIZE) FactorData {
    double momentum_20 = 0.0;
    double ep_ratio = 0.0;
    double log_mcap = 0.0;
    double volatility_20 = 0.0;
    double turnover_20 = 0.0;
    bool is_valid = false;
};*/
#include<vector>
#include<array>
constexpr const int FACTOR_NUM = 5; //因子数量
class StockDailyData;
class StockDailyFinancialData;
class StockDailyExtendedData;

class momentum_20;
class ep_ratio;
class log_mcap;
class volatility_20;
class turnover_20;


enum FactorType {
    MOMENTUM_20 = 0,
    EP_RATIO = 1,
    LOG_MCAP = 2,
    VOLATILITY_20 = 3,
    TURNOVER_20 = 4
};

class FactorBase
{
private:
	momentum_20* momentum_20_calculator;
	ep_ratio* ep_ratio_calculator;
    log_mcap* log_mcap_calculator;
	volatility_20* volatility_20_calculator;
	turnover_20* turnover_20_calculator;
public:
	momentum_20* get_momentum_20_calculator() const { return momentum_20_calculator; }
	ep_ratio* get_ep_ratio_calculator() const { return ep_ratio_calculator; }
	log_mcap* get_log_mcap_calculator() const { return log_mcap_calculator; }
	volatility_20* get_volatility_20_calculator() const { return volatility_20_calculator; }
	turnover_20* get_turnover_20_calculator() const { return turnover_20_calculator; }

    //存储每只股票的调仓日期在deque中的索引
	const std::vector<int> *rebalance_index; 
public:
    FactorBase(const int data_size,const std::array<bool,FACTOR_NUM> &is_valid);

	void update_factors(const std::vector<StockDailyData>& daily_datas,const std::vector<StockDailyExtendedData>& extended_datas, 
        const std::vector<StockDailyFinancialData>& financial_datas, const std::vector<int>& rebalance_index);
    ~FactorBase();
};