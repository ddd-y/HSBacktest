# CMakeList.txt: HSBacktest 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#

# 将源代码添加到此项目的可执行文件。
add_executable (HSBacktest "HSBacktest.cpp" "HSBacktest.h" "Datalevel/data_defs.h" "Datalevel/read_csvdata/read_csv.h" "MyLog/Logger.cpp" 
	"MyLog/Logger.h" "Datalevel/stock_k_data.h" "Datalevel/factor_calculate/factorbase.h" "Datalevel/factor_calculate/factorbase.cpp" 
	"Datalevel/factor_calculate/momentnum20/momentum_20.h" "Datalevel/factor_calculate/epratio/ep_ratio.h" "Datalevel/factor_calculate/logmcap/log_mcap.h"  
	"Datalevel/factor_calculate/volatility20/volatility_20.h" "Datalevel/factor_calculate/turnover20/turnover_20.h" 
	"Datalevel/factor_calculate/momentnum20/momentum_20.cpp" "Datalevel/factor_calculate/logmcap/log_mcap.cpp" 
	"Datalevel/factor_calculate/turnover20/turnover_20.cpp" "Datalevel/factor_calculate/volatility20/volatility_20.cpp" 
	"Datalevel/factor_calculate/epratio/ep_ratio.cpp" "Datalevel/Global_data.h" "Datalevel/Global_data.cpp" "Datalevel/stock_k_data.cpp"  
	"ConfigLvevl/configer.h" "ConfigLvevl/configer.cpp" "test/dataleveltest.h" "EngineLevel/TradeExecutor/TradeExecutor.h" "EngineLevel/TradeExecutor/TradeExecutor.cpp" "EngineLevel/StockSelector/StockSelector.h" 
	"EngineLevel/StockSelector/StockSelector.cpp" "EngineLevel/BacktestEngine.h" "EngineLevel/BacktestEngine.cpp" 
	"Datalevel/param_builder/param_builder.h" "Datalevel/param_builder/param_builder.cpp" "ParaLevel/multi_thread/multi_runner.h" "ParaLevel/multi_thread/multi_runner.cpp" 
	"CollectorLevel/performance_collector.h" "CollectorLevel/performance_collector.cpp" "CollectorLevel/BacktestSummary.h" "ParaLevel/multi_host/multi_host.h" "ParaLevel/multi_host/multi_host.cpp"
	"AnalysisLevel/analysis_engine.h" "AnalysisLevel/analysis_engine.cpp"
	"test/full_pipeline_test.h" "test/multithread_test.h" "test/functional_test.h" "test/multi_host_e2e_test.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HSBacktest PROPERTY CXX_STANDARD 20)
endif()

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(OpenMP REQUIRED)
find_package(MPI REQUIRED)
target_link_libraries(HSBacktest PUBLIC fmt::fmt spdlog::spdlog OpenMP::OpenMP_CXX MPI::MPI_CXX)
target_link_directories(HSBacktest PUBLIC ${CMAKE_SOURCE_DIR}/lib)
# TODO: 如有需要，请添加测试并安装目标。
