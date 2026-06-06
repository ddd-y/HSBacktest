// HSBacktest.cpp: 应用程序入口点 & 系统初始化
//
#include "HSBacktest.h"
#include "MyLog/Logger.h"
#include "Datalevel/Global_data.h"
#include "CollectorLevel/performance_collector.h"
#include "ParaLevel/multi_thread/multi_runner.h"
#include "ParaLevel/multi_host/multi_host.h"
#include "ConfigLvevl/configer.h"
#include"test/factor_score_test.h"
#include"test/backtest_unit_test.h"
#include"test/multi_host_test.h"
#include"test/multi_host_e2e_test.h"

// ===== 初始化 + 启动（配置从 Configer 读取）=====
bool InitAndRun()
{
	try
	{
		const auto& stock_files = Configer::GetStockDataFiles();
		bool use_mpi = Configer::GetUseMpi();

		// 1. 日志系统（最早初始化）
		HSBacktest::Logger::getInstance().init(Configer::GetLogPath());
		LOG_INFO("=== HSBacktest starting (mode={}) ===", use_mpi ? "MPI" : "single-machine");

		// 2. 全局数据（加载K线 + 计算因子 + 构建参数组合）
		LOG_INFO("Loading data for {} stocks...", stock_files.size());
		GlobalData::Init(stock_files);
		LOG_INFO("GlobalData: {} stocks, {} adjust-param combinations",
			GlobalData::GetGlobalData()->get_stock_count(),
			GlobalData::GetGlobalData()->GetAdjustParamCount());

		// 3. 性能收集器
		PerformanceCollector::Initialize();
		LOG_INFO("PerformanceCollector initialized");

		// 4. 启动回测
		if (use_mpi)
		{
			HostManager host_mgr;
			host_mgr.InitMPIRelated();
			host_mgr.distribute_task();
		}
		else
		{
			MultiRunner::MultiRun();
		}

		LOG_INFO("=== HSBacktest finished, {} total summaries ===",
			PerformanceCollector::GetPerformanceCollector()->GetTotalSummaryCount());

		GlobalData::Destroy();
		return true;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR("InitAndRun failed: {}", e.what());
		return false;
	}
}

// ===== 入口 =====
int main()
{
	Configer::LoadFromFile("config.json");

	if (!InitAndRun())
		return 1;
	return 0;
}
