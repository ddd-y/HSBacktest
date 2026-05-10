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
};