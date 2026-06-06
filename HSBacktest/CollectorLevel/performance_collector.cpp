#include "performance_collector.h"
#include <omp.h>

PerformanceCollector* PerformanceCollector::instance=nullptr;

PerformanceCollector::PerformanceCollector():summary_list(std::vector<std::deque<BacktestSummary>>(omp_get_num_procs()))
{}

void PerformanceCollector::AddSummary(const BacktestSummary & summary, int proc_serial)
{
	summary_list[proc_serial].push_back(summary);
}

std::vector<BacktestSummary> PerformanceCollector::TakeAllSummaries()
{
	std::vector<BacktestSummary> result;
	for (auto& dq : summary_list) {
		while (!dq.empty()) {
			result.push_back(dq.front());
			dq.pop_front();
		}
	}
	return result;
}

void PerformanceCollector::LoadAllSummaries(const std::vector<BacktestSummary>& summaries)
{
	// 清空所有 slot，将最终结果加载到 slot 0
	for (auto& dq : summary_list)
		dq.clear();
	if (summary_list.empty())
		summary_list.resize(1);
	for (const auto& s : summaries)
		summary_list[0].push_back(s);
}

