#pragma once
#include"stock_k_data.h"
#include"factor_calculate/factor_database.h"
#include"factor_calculate/factorbase.h"
#include"../ConfigLvevl/configer.h"
#include<vector>
#include<string>
#include"param_builder/param_builder.h"

//计算动量因子时需要前21天的数据
constexpr const int PRE_EXTRA_DAYS = 21;


// 全局数据类，负责存储所有股票回测所需要的K线数据
class GlobalData
{
private:
	std::vector<StockKData*> stock_k_datas;
	std::vector<FactorDatabase*> factor_databases;
	std::vector<FactorBase*> factor_bases;

	//调仓日在每只股票的K线数据中的索引位置，所有股票的调仓日索引位置相同
	std::vector<int> rebalance_index;
	// 所有股票的交易日期，按顺序存储，格式为YYYYMMDD，每个股票相同
	std::vector<int32_t> dates;
	//可调整因子数组
	std::vector<AdjustParam> adjust_params;
	// 执行所有股票因子计算
	void calculate_all_factors();

	static GlobalData* Globalinstance;
public:
	int GetAdjustParamCount() const { return static_cast<int>(adjust_params.size()); }
	
	GlobalData() = default;
	GlobalData(const std::vector<std::string>& stock_k_data_files);

	// 初始化全局数据类，只需要传入股票K线数据文件路径列表
	static void Init(const std::vector<std::string>& stock_k_data_files);

	/*
	* @brief 初始化可调整参数，每个进程分配不同参数组
	* @param process_size 进程数
	*/
	static void InitAdjustParams(int process_size);

	static void Destroy()
    {
        delete Globalinstance;
        Globalinstance = nullptr;
    }

	static GlobalData* GetGlobalData() { return Globalinstance; }

	FactorDatabase* get_factor_database(int index) const { return factor_databases[index]; }

	/*
	* @brief 获取第index_1个data的index_2个的5个因子值
	*/
	inline std::array<double, FACTOR_NUM> GetValue(int index_1, int index_2)
	{
		return std::array<double, FACTOR_NUM>({
			factor_databases[index_1]->get_momentum_20_data().get_momentum_20(index_2),
			factor_databases[index_1]->get_turnover_20_data().get_turnover_20(index_2),
			factor_databases[index_1]->get_volatility_20_data().get_volatility_20(index_2),
			factor_databases[index_1]->get_log_mcap_data().get_log_mcap(index_2),
			factor_databases[index_1]->get_ep_ratio_data().get_ep_ratio(index_2)
		});
	}

	/*
	* @brief 获取第index组因子权重，第i个数值对应GetValue的第i个因子
	* @param index 调整参数索引（同时用于 GetWeights 和 GetTopN）
	*/
	inline std::array<double, FACTOR_NUM> GetWeights(int index)
	{
		if (index >= 0 && index < static_cast<int>(adjust_params.size()))
			return adjust_params[index].factor_weights;

		// fallback：从 StrategyConfiger 读取默认值
		return std::array<double, FACTOR_NUM>({
			Configer::GetStrategyConfiger().GetMomentumWeight(),
			Configer::GetStrategyConfiger().GetTurnoverWeight(),
			Configer::GetStrategyConfiger().GetVolatilityWeight(),
			Configer::GetStrategyConfiger().GetMcapWeight(),
			Configer::GetStrategyConfiger().GetEpWeight()
		});
	}

	/*
	* @brief 获取第index组参数的选股数量
	* @param index 调整参数索引（同时用于 GetWeights 和 GetTopN）
	*/
	inline int GetTopN(int index)
	{
		if (index >= 0 && index < static_cast<int>(adjust_params.size()))
			return adjust_params[index].top_n;

		return Configer::GetStrategyConfiger().GetTopN();
	}

	

	FactorBase* get_factor_base(int index) const { return factor_bases[index]; }
	StockKData* get_stock_k_data(int index) const { return stock_k_datas[index]; }
	int get_stock_count() const { return static_cast<int>(stock_k_datas.size()); }

	const std::vector<int>& get_rebalance_index() const { return rebalance_index; }
	const std::vector<int32_t>& get_dates() const { return dates; }

	~GlobalData();
};