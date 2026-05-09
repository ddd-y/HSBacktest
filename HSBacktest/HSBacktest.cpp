// HSBacktest.cpp: 定义应用程序的入口点。
//

#include "HSBacktest.h"
#include"MyLog/Logger.h"
#include "Datalevel/read_csvdata/read_csv.h"
#include <fstream>
#include <iostream>
using namespace std;

void write_test_csv(const std::string& filename)
{
    std::ofstream ofs(filename);
    ofs << "trade_date,close,open,adj_factor,is_suspended,is_delisted,is_limit_up,is_limit_down"<<std::endl;
    ofs << "20250101,100.5,99.0,1.0,0,0,0,0"<<std::endl;
    ofs << "20250102,101.2,100.5,1.0,0,0,1,0"<<std::endl;
    ofs << "20250103,99.8,101.2,1.0,0,0,0,1"<<std::endl;
    ofs << "20250106,100.0,99.8,1.0,1,0,0,0"<<std::endl;
    ofs.close();
}

int main()
{
	HSBacktest::Logger::getInstance().init("./HSBacktest.log", spdlog::level::info);

    std::string test_file = "test_stock_daily.csv";
    write_test_csv(test_file);
    LOG_INFO("已生成测试CSV文件: {}", test_file);

    auto data = csvreader::read_stock_daily_data(test_file);
    LOG_INFO("成功解析 {} 条记录", data.size());

    for (const auto& row : data) {
        LOG_INFO("date={}, close={}, open={}, adj={}, suspended={}, delisted={}, limit_up={}, limit_down={}",
            row.trade_date, row.close, row.open, row.adj_factor,
            row.is_suspended, row.is_delisted, row.is_limit_up, row.is_limit_down);
    }

    return 0;
}
