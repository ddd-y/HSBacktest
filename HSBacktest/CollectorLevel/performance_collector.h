#pragma once
#include"BacktestSummary.h"
#include<deque>
#include<vector>
// ==========================================
// 回测绩效汇总
// ==========================================
class PerformanceCollector
{
private:
	static PerformanceCollector *instance;
	std::vector<std::deque<BacktestSummary>> summary_list;
public:
	PerformanceCollector();
	void AddSummary(const BacktestSummary& summary,int proc_serial);

	static PerformanceCollector* GetPerformanceCollector() { return PerformanceCollector::instance; }

	static void Initialize() { PerformanceCollector::instance = new PerformanceCollector(); }

	// 获取所有线程收集到的 summary 总数（供测试/统计用）
	int GetTotalSummaryCount() const {
		int total = 0;
		for (const auto& dq : summary_list) total += static_cast<int>(dq.size());
		return total;
	}
};