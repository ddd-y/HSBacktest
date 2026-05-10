#include "multi_runner.h"
#include"../../Datalevel/Global_data.h"
#include"../../EngineLevel/BacktestEngine.h"
#include"../../CollectorLevel/performance_collector.h"
#include<omp.h>

//初始资金，后边看看怎么改
constexpr const int NOW_CAPITAL = 10000;

void MultiRunner::MultiRun()
{
	omp_set_num_threads(omp_get_num_procs());
	int adjustparamnum = GlobalData::GetGlobalData()->GetAdjustParamCount();
	
	int cpu_num = omp_get_num_procs();
	std::vector<BacktestEngine*> Engines;
	for (int i = 0; i < cpu_num; ++i) 
	{
		BacktestEngine* engine=new BacktestEngine();
		engine->Initialize(NOW_CAPITAL);
        Engines.push_back(engine);
	}
	const int chunk_size = adjustparamnum / cpu_num;
#pragma omp parallel for num_threads(omp_get_num_procs()) schedule(static,chunk_size)
	for (int i = 0; i < adjustparamnum; ++i) 
	{
		const int cpu_serial_num = i / chunk_size % cpu_num;
		Engines[cpu_serial_num]->ReInitialize(i);
		Engines[cpu_serial_num]->Run();

		PerformanceCollector::GetPerformanceCollector()->AddSummary(Engines[cpu_serial_num]->GetSummary(),cpu_serial_num);
	}
}
