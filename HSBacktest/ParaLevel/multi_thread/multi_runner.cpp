#include "multi_runner.h"
#include"../../Datalevel/Global_data.h"
#include"../../EngineLevel/BacktestEngine.h"
#include"../../CollectorLevel/performance_collector.h"
#include"../../CollectorLevel/BacktestSummary.h"
#include<omp.h>
#include"../../MyLog/Logger.h"
#include<chrono>

// ===================================================================
// MultiHost 复用引擎池（文件静态变量）
// ===================================================================
static std::vector<BacktestEngine*> s_reuse_engines;
static double s_reuse_init_capital = 0.0;

void MultiRunner::MultiRun(int offset)
{
	int adjustparamnum = GlobalData::GetGlobalData()->GetAdjustParamCount();
	if (adjustparamnum <= 0) return;

	int cpu_num = omp_get_num_procs();
	if (cpu_num > adjustparamnum) cpu_num = adjustparamnum;

	omp_set_num_threads(cpu_num);
	LOG_INFO("{} threads start",cpu_num);
	std::vector<BacktestEngine*> Engines(cpu_num);

	double init_capital = Configer::GetInitCapital();
	for (int i = 0; i < cpu_num; ++i)
	{
		Engines[i] = new BacktestEngine();
		Engines[i]->Initialize(init_capital);
	}

	const int chunk_size = adjustparamnum / cpu_num;

	auto start_time = std::chrono::high_resolution_clock::now();

#pragma omp parallel for num_threads(cpu_num) schedule(static, chunk_size)
	for (int i = 0; i < adjustparamnum; ++i)
	{
		int tid = omp_get_thread_num();
		LOG_DEBUG("当前的线程号是：{}",tid);
		const int cpu_serial_num = i / chunk_size % cpu_num;
		Engines[cpu_serial_num]->ReInitialize(init_capital, i);
		Engines[cpu_serial_num]->Run();

		BacktestSummary s = Engines[cpu_serial_num]->GetSummary();
		s.param_index += offset;  // MPI 模式下 offset=task.start

		PerformanceCollector::GetPerformanceCollector()->AddSummary(s, cpu_serial_num);
	}

	auto end_time = std::chrono::high_resolution_clock::now();
	auto elapsed = std::chrono::duration<double>(end_time - start_time).count();
	double avg_time_per_param = elapsed / adjustparamnum;
	LOG_INFO("全部 {} 个参数处理完成，总耗时: {:.3f} 秒，平均每个参数: {:.3f} 秒", adjustparamnum, elapsed, avg_time_per_param);

	for (auto* e : Engines) delete e;
}

// ===================================================================
// MultiHost 专用：预创建引擎池
// ===================================================================
void MultiRunner::MultiRunInit(double init_capital)
{
	int total_cpus = omp_get_num_procs();
	if (total_cpus <= 0) total_cpus = 1;

	s_reuse_init_capital = init_capital;
	s_reuse_engines.resize(total_cpus);

	for (int i = 0; i < total_cpus; ++i)
	{
		s_reuse_engines[i] = new BacktestEngine();
		s_reuse_engines[i]->Initialize(init_capital);
	}

	LOG_INFO("MultiRunInit: {} engines created for reuse (init_capital={:.2f})", total_cpus, init_capital);
}

// ===================================================================
// MultiHost 专用：复用预创建引擎执行一轮回测
// ===================================================================
void MultiRunner::MultiRunReuse(int offset)
{
	int adjustparamnum = GlobalData::GetGlobalData()->GetAdjustParamCount();
	if (adjustparamnum <= 0) return;

	int total_cpus = static_cast<int>(s_reuse_engines.size());
	int active_threads = total_cpus;
	if (active_threads > adjustparamnum) active_threads = adjustparamnum;

	omp_set_num_threads(active_threads);
	LOG_DEBUG("MultiRunReuse: {} / {} engines active for {} params", active_threads, total_cpus, adjustparamnum);

	const int chunk_size = adjustparamnum / active_threads;

	auto start_time = std::chrono::high_resolution_clock::now();

#pragma omp parallel for num_threads(active_threads) schedule(static, chunk_size)
	for (int i = 0; i < adjustparamnum; ++i)
	{
		int tid = omp_get_thread_num();
		LOG_DEBUG("当前的线程号是：{}", tid);
		const int cpu_serial_num = i / chunk_size % active_threads;
		s_reuse_engines[cpu_serial_num]->ReInitialize(s_reuse_init_capital, i);
		s_reuse_engines[cpu_serial_num]->Run();

		BacktestSummary s = s_reuse_engines[cpu_serial_num]->GetSummary();
		s.param_index += offset;

		PerformanceCollector::GetPerformanceCollector()->AddSummary(s, cpu_serial_num);
	}

	auto end_time = std::chrono::high_resolution_clock::now();
	auto elapsed = std::chrono::duration<double>(end_time - start_time).count();
	double avg_time_per_param = elapsed / adjustparamnum;
	LOG_INFO("全部 {} 个参数处理完成，总耗时: {:.3f} 秒，平均每个参数: {:.3f} 秒", adjustparamnum, elapsed, avg_time_per_param);
}

// ===================================================================
// MultiHost 专用：销毁引擎池
// ===================================================================
void MultiRunner::MultiRunCleanup()
{
	for (auto* e : s_reuse_engines) delete e;
	s_reuse_engines.clear();
	LOG_INFO("MultiRunCleanup: all reuse engines destroyed");
}
