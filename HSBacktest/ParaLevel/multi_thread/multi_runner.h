#pragma once
#include<vector>

class BacktestEngine;

class MultiRunner 
{
public:
	// @param offset  全局参数起始索引（单机=0，MPI=task.start）
	static void MultiRun(int offset = 0);

	// ---- 以下为 MultiHost 专用：复用引擎池避免反复 new/delete ----
	// 预创建引擎池（应在 work_stealing_loop 前调用一次）
	static void MultiRunInit(double init_capital);
	// 复用预创建的引擎执行一轮回测
	static void MultiRunReuse(int offset);
	// 销毁引擎池（应在 work_stealing_loop 后调用一次）
	static void MultiRunCleanup();
};