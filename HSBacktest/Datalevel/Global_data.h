#pragma once
#include"stock_k_data.h"
#include<vector>
#include<string>
class GlobalData
{
private:
	std::vector<StockKData*> stock_k_datas; // 存储所有股票回测所需要的K线数据
	StockDailyTransactionCost TransactionCost; // 存储交易成本数据,全局唯一
public:
	GlobalData() = default;
	GlobalData(const std::vector<std::string>& stock_k_data_files);
};