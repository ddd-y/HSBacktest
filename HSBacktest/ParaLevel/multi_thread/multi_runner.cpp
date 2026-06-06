#include "multi_runner.h"
#include"../../Datalevel/Global_data.h"
#include"../../EngineLevel/BacktestEngine.h"
#include"../../CollectorLevel/performance_collector.h"
#include"../../CollectorLevel/BacktestSummary.h"
#include<omp.h>
#include"../../MyLog/Logger.h"

void MultiRunner::MultiRun(int offset)
{
	int adjustparamnum = GlobalData::GetGlobalData()->GetAdjustParamCount();
	if (adjustparamnum <= 0) return;

	int cpu_num = omp_get_num_procs();
	if (cpu_num > adjustparamnum) cpu_num = adjustparamnum;

	omp_set_num_threads(cpu_num);

	std::vector<BacktestEngine*> Engines(cpu_num);

	double init_capital = Configer::GetInitCapital();
	for (int i = 0; i < cpu_num; ++i)
	{
		Engines[i] = new BacktestEngine();
		Engines[i]->Initialize(init_capital);
	}

	const int chunk_size = adjustparamnum / cpu_num;

#pragma omp parallel for num_threads(cpu_num) schedule(static, chunk_size)
	for (int i = 0; i < adjustparamnum; ++i)
	{
		int tid = omp_get_thread_num();
		LOG_INFO("当前的线程号是：{}",tid);
		const int cpu_serial_num = i / chunk_size % cpu_num;
		Engines[cpu_serial_num]->ReInitialize(init_capital, i);
		Engines[cpu_serial_num]->Run();

		BacktestSummary s = Engines[cpu_serial_num]->GetSummary();
		s.param_index += offset;  // MPI 模式下 offset=task.start

		PerformanceCollector::GetPerformanceCollector()->AddSummary(s, cpu_serial_num);
	}

	for (auto* e : Engines) delete e;
}
