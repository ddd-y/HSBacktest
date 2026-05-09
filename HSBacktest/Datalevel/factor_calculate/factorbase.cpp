#include"factorbase.h"
#include"../../MyLog/Logger.h"
#include"momentnum20/momentum_20.h"
#include"epratio/ep_ratio.h"
#include"logmcap/log_mcap.h"
#include"turnover20/turnover_20.h"
#include"volatility20/volatility_20.h"

FactorBase::FactorBase(FactorDatabase& database, const std::array<bool, FACTOR_NUM>& is_valid)
{
	momentum_20_calculator = nullptr;
	ep_ratio_calculator = nullptr;
	log_mcap_calculator = nullptr;
	volatility_20_calculator = nullptr;
	turnover_20_calculator = nullptr;

	if (is_valid[FactorType::MOMENTUM_20]) {
		momentum_20_calculator = new momentum_20(database.get_momentum_20_data());
	}
	if (is_valid[FactorType::EP_RATIO]) {
		ep_ratio_calculator = new ep_ratio(database.get_ep_ratio_data());
	}
	if (is_valid[FactorType::LOG_MCAP]) {
		log_mcap_calculator = new log_mcap(database.get_log_mcap_data());
	}
	if (is_valid[FactorType::VOLATILITY_20]) {
		volatility_20_calculator = new volatility_20(database.get_volatility_20_data());
	}
	if (is_valid[FactorType::TURNOVER_20]) {
		turnover_20_calculator = new turnover_20(database.get_turnover_20_data());
	}
}

void FactorBase::update_factors(const std::vector<StockDailyData>& daily_datas, const std::vector<StockDailyExtendedData>& extended_datas, const std::vector<StockDailyFinancialData>& financial_datas, const std::vector<int>& rebalance_index)
{
	if (momentum_20_calculator)
		momentum_20_calculator->update_momentum_20(daily_datas, rebalance_index);
	if(log_mcap_calculator)
		log_mcap_calculator->update_log_mcap(daily_datas, financial_datas ,rebalance_index);
	if(ep_ratio_calculator)
		ep_ratio_calculator->update_ep_ratio(daily_datas, financial_datas, rebalance_index);
	if(volatility_20_calculator)
		volatility_20_calculator->update_volatility_20(daily_datas, rebalance_index);
	if (turnover_20_calculator)
		turnover_20_calculator->update_turnover_20(extended_datas, financial_datas ,rebalance_index);
}

FactorBase::~FactorBase()
{
	if (momentum_20_calculator)
		delete momentum_20_calculator;
	if (ep_ratio_calculator)
		delete ep_ratio_calculator;
	if (log_mcap_calculator)
		delete log_mcap_calculator;
	if (volatility_20_calculator)
		delete volatility_20_calculator;
	if (turnover_20_calculator)
		delete turnover_20_calculator;
}


