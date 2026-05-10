#include"factorbase.h"
#include"../../MyLog/Logger.h"
#include"momentnum20/momentum_20.h"
#include"epratio/ep_ratio.h"
#include"logmcap/log_mcap.h"
#include"turnover20/turnover_20.h"
#include"volatility20/volatility_20.h"

FactorBase::FactorBase(FactorDatabase& database)
{
	momentum_20_calculator = new momentum_20(database.get_momentum_20_data());
	ep_ratio_calculator = new ep_ratio(database.get_ep_ratio_data());
	log_mcap_calculator = new log_mcap(database.get_log_mcap_data());
	volatility_20_calculator = new volatility_20(database.get_volatility_20_data());
	turnover_20_calculator = new turnover_20(database.get_turnover_20_data());
}

void FactorBase::update_factors(const std::vector<StockDailyData>& daily_datas, const std::vector<StockDailyExtendedData>& extended_datas, const std::vector<StockDailyFinancialData>& financial_datas, const std::vector<int>& rebalance_index)
{
	momentum_20_calculator->update_momentum_20(daily_datas, rebalance_index);
	log_mcap_calculator->update_log_mcap(daily_datas, financial_datas, rebalance_index);
	ep_ratio_calculator->update_ep_ratio(daily_datas, financial_datas, rebalance_index);
	volatility_20_calculator->update_volatility_20(daily_datas, rebalance_index);
	turnover_20_calculator->update_turnover_20(extended_datas, financial_datas, rebalance_index);
}

FactorBase::~FactorBase()
{
	delete momentum_20_calculator;
	delete ep_ratio_calculator;
	delete log_mcap_calculator;
	delete volatility_20_calculator;
	delete turnover_20_calculator;
}


