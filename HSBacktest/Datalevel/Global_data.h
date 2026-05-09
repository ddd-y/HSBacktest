#pragma once
#include"stock_k_data.h"
#include"factor_calculate/factor_database.h"
#include"factor_calculate/factorbase.h"
#include<vector>
#include<string>

// 全局数据类，负责存储所有股票回测所需要的K线数据
class GlobalData
{
private:
	std::vector<StockKData*> stock_k_datas;
	std::vector<FactorDatabase*> factor_databases;
	std::vector<FactorBase*> factor_bases;

	std::array<bool, FACTOR_NUM> factor_is_valid;

	// 执行所有股票因子计算
	void calculate_all_factors();
public:
	GlobalData() = default;
	GlobalData(const std::vector<std::string>& stock_k_data_files);

	FactorDatabase* get_factor_database(int index) const { return factor_databases[index]; }
	FactorBase* get_factor_base(int index) const { return factor_bases[index]; }
	StockKData* get_stock_k_data(int index) const { return stock_k_datas[index]; }
	int get_stock_count() const { return static_cast<int>(stock_k_datas.size()); }

	~GlobalData();
};