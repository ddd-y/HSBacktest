#include "Global_data.h"
#include"data_defs.h"
#include"read_csvdata/read_csv.h"


GlobalData::GlobalData(const std::vector<std::string>& stock_k_data_files)
{
	for (const std::string& a : stock_k_data_files) 
	{
		stock_k_datas.push_back(new StockKData());
	}
}
