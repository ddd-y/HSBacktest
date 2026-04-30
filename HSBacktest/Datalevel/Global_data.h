#pragma once
#include"stock_k_data.h"
#include<vector>
#include<string>

// 全局数据类，负责存储所有股票回测所需要的K线数据
class GlobalData
{
private:
	std::vector<StockKData*> stock_k_datas; // 存储所有股票回测所需要的K线数据
public:
	GlobalData() = default;
	GlobalData(const std::vector<std::string>& stock_k_data_files);
};