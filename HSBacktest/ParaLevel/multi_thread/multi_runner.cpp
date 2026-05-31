#include "multi_runner.h"
#include"../../Datalevel/Global_data.h"
#include"../../EngineLevel/BacktestEngine.h"
#include"../../CollectorLevel/performance_collector.h"
#include<omp.h>
#include"../../MyLog/Logger.h"

//初始资金，后边看看怎么改
constexpr const int NOW_CAPITAL = 10000;

void MultiRunner::MultiRun()
{
	int adjustparamnum = GlobalData::GetGlobalData()->GetAdjustParamCount();
	if (adjustparamnum <= 0) return;

	int cpu_num = omp_get_num_procs();
	if (cpu_num > adjustparamnum) cpu_num = adjustparamnum;

	omp_set_num_threads(cpu_num);

	std::vector<BacktestEngine*> Engines(cpu_num);
	for (int i = 0; i < cpu_num; ++i)
	{
		Engines[i] = new BacktestEngine();
		Engines[i]->Initialize(NOW_CAPITAL);
	}

	const int chunk_size = adjustparamnum / cpu_num;  // >= 1

#pragma omp parallel for num_threads(cpu_num) schedule(static, chunk_size)
	for (int i = 0; i < adjustparamnum; ++i)
	{
		int tid = omp_get_thread_num();
		LOG_INFO("当前的线程号是：{}",tid);
		const int cpu_serial_num = i / chunk_size % cpu_num;
		Engines[cpu_serial_num]->ReInitialize(i);
		Engines[cpu_serial_num]->Run();

		PerformanceCollector::GetPerformanceCollector()->AddSummary(
			Engines[cpu_serial_num]->GetSummary(), cpu_serial_num);
	}

	for (auto* e : Engines) delete e;
}
