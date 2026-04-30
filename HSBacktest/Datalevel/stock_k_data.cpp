#include "stock_k_data.h"
#include"data_defs.h"
#include"factor_calculate/factorbase.h"
#include"read_csvdata/read_csv.h"
#include"../ConfigLvevl/configer.h"

StockKData::StockKData(const std::string& code) :stock_code(code)
{
	daily_datas = std::move(csvreader::read_stock_daily_data(code + "_daily.csv"));
	extended_datas = std::move(csvreader::read_stock_daily_extended_data(code + "_daily_extended.csv"));
	financial_datas = std::move(csvreader::read_stock_daily_financial_data(code + "_daily_financial.csv"));

	// 初始化FactorBase
	const int changeduration = Configer::GetDataLevelConfiger().GetChangeDuration();
	for (int i = PRE_EXTRA_DAYS; i < daily_datas.size(); i += changeduration)
	{
		rebalance_index.push_back(i);
	}
	//后边要改
	factor_base = new FactorBase(rebalance_index.size(), {true,true,true,true,true});
}

inline void StockKData::calculate_factors()
{
	factor_base->update_factors(daily_datas, extended_datas, financial_datas, rebalance_index);
}
