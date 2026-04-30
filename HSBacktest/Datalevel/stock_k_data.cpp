#include "stock_k_data.h"
#include"data_defs.h"
#include"factor_calculate/factorbase.h"
#include"read_csvdata/read_csv.h"

StockKData::StockKData(const std::string& code) :stock_code(code)
{
	daily_datas = std::move(csvreader::read_stock_daily_data(code + "_daily.csv"));
	extended_datas = std::move(csvreader::read_stock_daily_extended_data(code + "_daily_extended.csv"));
	financial_datas = std::move(csvreader::read_stock_daily_financial_data(code + "_daily_financial.csv"));
}
