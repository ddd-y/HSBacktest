# CMakeList.txt: HSBacktest 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#

# 将源代码添加到此项目的可执行文件。
add_executable (HSBacktest "HSBacktest.cpp" "HSBacktest.h" "Datalevel/data_defs.h" "Datalevel/read_csvdata/read_csv.h" "MyLog/Logger.cpp" "MyLog/Logger.h" "Datalevel/stock_k_data.h" "Datalevel/factor_calculate/factorbase.h" "Datalevel/factor_calculate/factorbase.cpp" "Datalevel/factor_calculate/momentnum20/momentum_20.h" "Datalevel/factor_calculate/epratio/ep_ratio.h" "Datalevel/factor_calculate/logmcap/log_mcap.h"  "Datalevel/factor_calculate/volatility20/volatility_20.h" "Datalevel/factor_calculate/turnover20/turnover_20.h" "Datalevel/factor_calculate/momentnum20/momentum_20.cpp" "Datalevel/factor_calculate/logmcap/log_mcap.cpp" "Datalevel/factor_calculate/turnover20/turnover_20.cpp" "Datalevel/factor_calculate/volatility20/volatility_20.cpp" "Datalevel/factor_calculate/epratio/ep_ratio.cpp" "Datalevel/Global_data.h" "Datalevel/Global_data.cpp" "Datalevel/stock_k_data.cpp"  "ConfigLvevl/configer.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET HSBacktest PROPERTY CXX_STANDARD 20)
endif()

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
target_link_libraries(HSBacktest PUBLIC fmt::fmt spdlog::spdlog)
target_link_directories(HSBacktest PUBLIC ${CMAKE_SOURCE_DIR}/lib)
# TODO: 如有需要，请添加测试并安装目标。
