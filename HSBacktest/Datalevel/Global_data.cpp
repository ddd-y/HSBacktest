#include "Global_data.h"
#include"data_defs.h"
#include"read_csvdata/read_csv.h"
#include"../ConfigLvevl/configer.h"


GlobalData::GlobalData(const std::vector<std::string>& stock_k_data_files)
{
	// 从configer获取因子有效性配置
	factor_is_valid = {
		Configer::GetDataLevelConfiger().GetMomentum20Enabled(),
		Configer::GetDataLevelConfiger().GetTurnover20Enabled(),
		Configer::GetDataLevelConfiger().GetVolatility20Enabled(),
		Configer::GetDataLevelConfiger().GetLogMcapEnabled(),
		Configer::GetDataLevelConfiger().GetEpRatioEnabled()
	};

	// 第一步：读取所有股票数据
	for (const std::string& code : stock_k_data_files)
	{
		stock_k_datas.push_back(new StockKData(code));
	}

	const int stock_num = stock_k_datas.size();

	// 第二步：创建FactorDatabase数组（每只股票一个）
	for (int i = 0; i < stock_num; ++i)
	{
		int data_size = stock_k_datas[i]->get_rebalance_count();
		factor_databases.push_back(new FactorDatabase(data_size));
	}

	// 第三步：创建FactorBase数组（每只股票一个）
	for (int i = 0; i < stock_num; ++i)
	{
		factor_bases.push_back(new FactorBase(*factor_databases[i], factor_is_valid));
	}

	// 第四步：计算所有因子
	calculate_all_factors();
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
			stock_data->get_rebalance_index()
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
