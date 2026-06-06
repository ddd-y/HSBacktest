#include "Global_data.h"
#include"data_defs.h"
#include"read_csvdata/read_csv.h"
#include"../ConfigLvevl/configer.h"
#include"param_builder/param_builder.h"

GlobalData* GlobalData::Globalinstance=nullptr;

GlobalData::GlobalData(const std::vector<std::string>& stock_k_data_files)
{
	// 第一步：读取所有股票数据并计算rebalance_index，存储dates
	for (const std::string& code : stock_k_data_files)
	{
		stock_k_datas.push_back(new StockKData(code));
	}
	if (stock_k_datas.empty())
	{
		LOG_ERROR("No stock data files provided.");
		throw std::runtime_error("No stock data files provided.");
	}

	const int total_days = stock_k_datas[0]->get_daily_datas().size();
	dates.reserve(total_days);
	for (int i = 0; i < total_days; ++i)
	{
		dates.push_back(stock_k_datas[0]->get_daily_datas()[i].trade_date);
	}

	const int hold_days = Configer::GetStrategyConfiger().GetHoldDays();
	for (int i = PRE_EXTRA_DAYS; i < total_days; i += hold_days)
	{
		if (i == total_days-1)
			continue;
		rebalance_index.push_back(i);
	}

	const int stock_num = stock_k_datas.size();

	// 第二步：创建FactorDatabase数组（每只股票一个）
	for (int i = 0; i < stock_num; ++i)
	{
		factor_databases.push_back(new FactorDatabase(total_days));
	}

	// 第三步：创建FactorBase数组（每只股票一个）
	for (int i = 0; i < stock_num; ++i)
	{
		factor_bases.push_back(new FactorBase(*factor_databases[i]));
	}

	// 第四步：计算所有因子
	calculate_all_factors();

	//第五步，构建参数
	ParamBuilder::BuildParamNet(adjust_params, Configer::GetParamSearchConfig());
	params_end = static_cast<int>(adjust_params.size());  // 默认全量范围，MPI 子节点可通过 MPI_ChangeDataRange 覆盖
	LOG_INFO("GlobalData: built {} adjust parameter combinations", adjust_params.size());
}

void GlobalData::Init(const std::vector<std::string>& stock_k_data_files)
{
	Globalinstance=new GlobalData(stock_k_data_files);
}


void GlobalData::calculate_all_factors()
{
	const int stock_num = stock_k_datas.size();
#pragma omp parallel for schedule(dynamic)
	for (int i = 0; i < stock_num; ++i)
	{
		StockKData* stock_data = stock_k_datas[i];
		FactorBase* factor_base = factor_bases[i];
		
		factor_base->update_factors(
			stock_data->get_daily_datas(),
			stock_data->get_extended_datas(),
			stock_data->get_financial_datas(),
			rebalance_index
		);
	}
}


GlobalData::~GlobalData()
{
	for (auto* fb : factor_bases)
	{
		delete fb;
	}
	for (auto* fd : factor_databases)
	{
		delete fd;
	}
	for (auto* skd : stock_k_datas)
	{
		delete skd;
	}
}
