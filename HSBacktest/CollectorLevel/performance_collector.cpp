#include "performance_collector.h"
#include <omp.h>

PerformanceCollector* PerformanceCollector::instance=nullptr;

PerformanceCollector::PerformanceCollector():summary_list(std::vector<std::deque<BacktestSummary>>(omp_get_num_procs()))
{}

void PerformanceCollector::AddSummary(const BacktestSummary & summary, int proc_serial)
{
	summary_list[proc_serial].push_back(summary);
}

