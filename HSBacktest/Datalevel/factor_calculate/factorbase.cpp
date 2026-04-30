#include"factorbase.h"
#include"../../MyLog/Logger.h"
#include"momentnum20/momentum_20.h"
#include"epratio/ep_ratio.h"
#include"logmcap/log_mcap.h"
#include"turnover20/turnover_20.h"
#include"volatility20/volatility_20.h"

FactorBase::FactorBase(const int data_size, const std::array<bool, FACTOR_NUM>& is_valid)
{
	for (int i = 0; i < FACTOR_NUM; ++i)
	{
		switch (i)
		{
		case FactorType::MOMENTUM_20:
			if (is_valid[i])
				momentum_20_calculator = new momentum_20(data_size);
			else
				momentum_20_calculator = nullptr;
			break;
		case FactorType::LOG_MCAP:
			if (is_valid[i])
				log_mcap_calculator = new log_mcap(data_size);
			else
				log_mcap_calculator = nullptr;
		case FactorType::EP_RATIO:
			if (is_valid[i])
				ep_ratio_calculator = new ep_ratio(data_size);
			else
				ep_ratio_calculator = nullptr;
			break;
		case FactorType::VOLATILITY_20:
			if (is_valid[i])
				volatility_20_calculator = new volatility_20(data_size);
			else
				volatility_20_calculator = nullptr;
			break;
		case FactorType::TURNOVER_20:
			if (is_valid[i])
				turnover_20_calculator = new turnover_20(data_size);
			else
				turnover_20_calculator = nullptr;
			break;
		default:
			break;
		}
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
	if(momentum_20_calculator)
		delete momentum_20_calculator;
	if(log_mcap_calculator)
		delete log_mcap_calculator;
	if (ep_ratio_calculator)
		delete ep_ratio_calculator;
	if (volatility_20_calculator)
		delete volatility_20_calculator;
	if (turnover_20_calculator)
		delete turnover_20_calculator;
}


