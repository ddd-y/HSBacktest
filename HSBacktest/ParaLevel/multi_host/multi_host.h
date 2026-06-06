#pragma once
#include<openmpi/mpi.h>
#include<deque>
#include<vector>

// MPI 动态分配任务，一次分配的数量
constexpr const int UNIT_TASK_SIZE = 512;

// MPI 自定义类型：任务区间 [start, end]
struct TaskRange
{
	int start;
	int end;
};

// MPI Tag 常量
constexpr int TAG_STEAL_REQ  = 2;   // 窃取请求（thief → victim）
constexpr int TAG_STEAL_RESP = 3;   // 窃取响应（victim → thief）
constexpr int TAG_DONE       = 4;   // 进程报告完成（→ rank 0）
constexpr int TAG_TERM       = 5;   // 全局终止广播（rank 0 → all）

class HostManager
{
private:
	// MPI 的 rank 号
	int rank = 0;
	// 一共有多少进程
	int rank_size = 0;
	// MPI 自定义类型（TaskRange）
	MPI_Datatype* mpi_task_range_type = nullptr;
	// MPI 自定义类型（BacktestSummary）
	MPI_Datatype* mpi_summary_type = nullptr;

	// ---- 工作窃取状态 ----
	// 本地任务队列（后端 push/pop = LIFO 给自己；前端窃取 = FIFO 给他人）
	std::deque<TaskRange> task_queue_;

	// 窃取状态
	bool     steal_pending_ = false;   // 是否有未完成的窃取请求
	int      steal_target_  = -1;      // 正在向谁窃取
	MPI_Request steal_req_;            // 窃取请求的 MPI_Isend handle

	// 终止状态
	bool     i_am_done_     = false;   // 我已向 rank 0 报告完成
	bool     globally_done_ = false;   // 收到全局终止广播

	// 哪些 victim 已被证实为空（避免重复窃取）
	std::vector<bool> victim_empty_;

	// 是否已调用 MPI_Init（析构时据此判断是否需要 MPI_Finalize）
	bool mpi_initialized_ = false;

public:
	~HostManager();

	// 启动 MPI 相关
	void InitMPIRelated();
	// 任务分发入口：Phase1 工作窃取 → Phase2 MPI_Gatherv
	void distribute_task();

#ifndef NDEBUG
	// ===== 测试辅助（仅 Debug 构建）=====
	void SetRankAndSizeForTest(int r, int sz) { rank = r; rank_size = sz; }
	int  GetRank()          const { return rank; }
	int  GetRankSize()      const { return rank_size; }
	bool IsGloballyDone()   const { return globally_done_; }
	bool IsDone()           const { return i_am_done_; }
	const std::vector<bool>&       GetVictimEmpty() const { return victim_empty_; }
	const std::deque<TaskRange>&   GetTaskQueue()   const { return task_queue_; }

	// 暴露私有方法（wrapper）
	void BuildInitialTasksForTest(int n)     { build_initial_tasks(n); }
	int  PickRandomVictimForTest()           { return pick_random_victim(); }
	bool AllVictimsKnownEmptyForTest() const { return all_victims_known_empty(); }
#endif

private:
	// 所有进程平等参与的工作窃取循环
	void work_stealing_loop();

	// 结果收集（保持 MPI_Gatherv）
	void gather_results();

	// ---- 窃取内部方法 ----
	// 构建本地任务队列：总参数空间平均分配
	void build_initial_tasks(int total_params);

	// 处理所有待处理的窃取请求（外来）
	void service_incoming_steals();

	// 发起一次窃取请求
	void initiate_steal();

	// 检查窃取响应是否到达
	void check_steal_response();

	// 检查是否所有 victim 都试过且为空
	bool all_victims_known_empty() const;

	// 选择一个随机 victim（跳过已知为空的），返回 -1 表示无可用 victim
	int  pick_random_victim();

	// 向 rank 0 报告完成
	void report_done();

	// 检查 rank 0 是否广播了终止
	bool poll_termination();
};
