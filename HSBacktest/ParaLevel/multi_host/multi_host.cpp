#include "multi_host.h"
#include"../../Datalevel/Global_data.h"
#include"../../CollectorLevel/BacktestSummary.h"
#include"../../CollectorLevel/performance_collector.h"
#include"../multi_thread/multi_runner.h"
#include"../../ConfigLvevl/configer.h"
#include"../../MyLog/Logger.h"
#include<algorithm>
#include<cstddef>
#include<cstdlib>
#include<ctime>

// ===================================================================
// MPI 初始化：注册自定义数据类型
// ===================================================================
void HostManager::InitMPIRelated()
{
	MPI_Init(NULL, NULL);
	mpi_initialized_ = true;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &rank_size);

	// 注册自定义 MPI 类型 TaskRange { int start; int end; }
	mpi_task_range_type = new MPI_Datatype;
	int blocklengths[2] = { 1, 1 };
	MPI_Aint offsets[2];
	MPI_Datatype types[2] = { MPI_INT, MPI_INT };

	offsets[0] = offsetof(TaskRange, start);
	offsets[1] = offsetof(TaskRange, end);

	MPI_Type_create_struct(2, blocklengths, offsets, types, mpi_task_range_type);
	MPI_Type_commit(mpi_task_range_type);

	// 注册自定义 MPI 类型 BacktestSummary
	mpi_summary_type = new MPI_Datatype;
	int s_blocklengths[7] = { 1,1,1,1,1,1,1 };
	MPI_Aint s_offsets[7];
	MPI_Datatype s_types[7] = {
		MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE,
		MPI_DOUBLE, MPI_INT
	};

	s_offsets[0] = offsetof(BacktestSummary, total_return);
	s_offsets[1] = offsetof(BacktestSummary, annual_return);
	s_offsets[2] = offsetof(BacktestSummary, annual_volatility);
	s_offsets[3] = offsetof(BacktestSummary, sharpe_ratio);
	s_offsets[4] = offsetof(BacktestSummary, max_drawdown);
	s_offsets[5] = offsetof(BacktestSummary, avg_turnover);
	s_offsets[6] = offsetof(BacktestSummary, param_index);

	MPI_Type_create_struct(7, s_blocklengths, s_offsets, s_types, mpi_summary_type);
	MPI_Type_commit(mpi_summary_type);
}

// ===================================================================
// 析构：释放 MPI 资源
// ===================================================================
HostManager::~HostManager()
{
	if (mpi_task_range_type)
	{
		MPI_Type_free(mpi_task_range_type);
		delete mpi_task_range_type;
		mpi_task_range_type = nullptr;
	}
	if (mpi_summary_type)
	{
		MPI_Type_free(mpi_summary_type);
		delete mpi_summary_type;
		mpi_summary_type = nullptr;
	}
	if (mpi_initialized_)
		MPI_Finalize();
}

// ===================================================================
// 任务分发入口
//     Phase 1: work_stealing_loop（所有进程对等，批量窃取）
//     Phase 2: gather_results（MPI_Gatherv）
// ===================================================================
void HostManager::distribute_task()
{
	// ---- Phase 1: 工作窃取 ----
	work_stealing_loop();

	// ---- Phase 2: 结果收集 ----
	MPI_Barrier(MPI_COMM_WORLD);   // 确保所有进程退出窃取循环
	gather_results();

	if (rank == 0)
	{
		LOG_INFO("MPI rank 0: work stealing complete, {} total summaries",
			PerformanceCollector::GetPerformanceCollector()->GetTotalSummaryCount());
	}
}

// ===================================================================
// 将总参数空间均匀切分为 TaskRange 块，平均分配给所有进程
// ===================================================================
void HostManager::build_initial_tasks(int total_params)
{
	task_queue_.clear();

	if (total_params <= 0) return;

	// 生成所有任务块
	std::vector<TaskRange> all_tasks;
	for (int start = 0; start < total_params; start += UNIT_TASK_SIZE)
	{
		TaskRange t;
		t.start = start;
		t.end   = std::min(start + UNIT_TASK_SIZE, total_params);
		all_tasks.push_back(t);
	}

	int total_tasks = static_cast<int>(all_tasks.size());
	int base        = total_tasks / rank_size;
	int remainder   = total_tasks % rank_size;

	// 本进程分得 base 个（若 rank < remainder 则多分 1 个）
	int my_start = rank * base + std::min(rank, remainder);
	int my_count = base + (rank < remainder ? 1 : 0);

	for (int i = 0; i < my_count; ++i)
		task_queue_.push_back(all_tasks[my_start + i]);

	// 初始化 victim_empty_ 标记（全部未知）
	victim_empty_.assign(rank_size, false);
	victim_empty_[rank] = true;  // 不偷自己

	LOG_INFO("MPI rank {}: initialized with {}/{} tasks", rank, my_count, total_tasks);
}

// ===================================================================
// Phase 1 — 批量工作窃取主循环
//
//   所有进程（包括 rank 0）运行同一逻辑：
//     1. 先处理外来窃取请求  （MPI_Iprobe，绝不死锁）
//     2. 若有任务：pop 一个，计算
//     3. 若任务空：随机窃取 （MPI_Isend → MPI_Iprobe 等响应）
//     4. 全部 victim 为空 → 报告 DONE，等待全局终止
// ===================================================================
void HostManager::work_stealing_loop()
{
	GlobalData* gd         = GlobalData::GetGlobalData();
	int         total_params = gd->GetAdjustParamCount();

	LOG_INFO("MPI rank {}: work-stealing loop started, total_params={}", rank, total_params);

	// ---- 平均分配初始任务 ----
	build_initial_tasks(total_params);
	if (total_params <= 0)
	{
		// 无任务：所有进程直接退出，由 barrier + gather 收尾
		return;
	}

	// ---- rank 0 维护的完成计数器 ----
	int done_count = 0;

	// ---- 随机数种子（用于选 victim）----
	std::srand(static_cast<unsigned>(rank) * 12345u +
	           static_cast<unsigned>(std::time(nullptr)) % 10007u);

	// ---- 主循环 ----
	while (!globally_done_)
	{
		// =========================================
		// 1. 先服务外来窃取请求（最高优先级，防死锁）
		// =========================================
		service_incoming_steals();

		// =========================================
		// 2. rank 0：收齐所有待处理的 DONE 报告
		// =========================================
		if (rank == 0)
		{
			int flag = 0;
			MPI_Status status;
			while (true)
			{
				MPI_Iprobe(MPI_ANY_SOURCE, TAG_DONE, MPI_COMM_WORLD, &flag, &status);
				if (!flag) break;

				int dummy;
				MPI_Recv(&dummy, 1, MPI_INT, status.MPI_SOURCE,
				         TAG_DONE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				done_count++;
				LOG_INFO("MPI rank 0: received DONE from rank {} (total done={}/{})",
					status.MPI_SOURCE, done_count, rank_size);
			}
		}

		// =========================================
		// 3. 检查全局终止（rank 0 操作，或任意进程接收广播）
		// =========================================
		if (poll_termination())
		{
			LOG_INFO("MPI rank {}: received termination, exiting loop", rank);
			break;
		}

		// rank 0：检查自身 + 所有其他进程是否都已 DONE
		if (rank == 0 && i_am_done_ && done_count == rank_size - 1)
		{
			// 广播终止
			LOG_INFO("MPI rank 0: all {} processes done, broadcasting termination", rank_size);
			for (int i = 1; i < rank_size; ++i)
				MPI_Send(nullptr, 0, MPI_INT, i, TAG_TERM, MPI_COMM_WORLD);
			globally_done_ = true;
			break;
		}

		// =========================================
		// 4. 如果我已经报告 DONE，只做上述收尾工作
		// =========================================
		if (i_am_done_)
			continue;

		// =========================================
		// 5. 有任务 → 自己算
		// =========================================
		if (!task_queue_.empty())
		{
			// 从后端 pop（LIFO 给自己 = 经典 work-stealing deque 约定）
			TaskRange task = task_queue_.back();
			task_queue_.pop_back();

			LOG_INFO("MPI rank {}: computing task [{}, {}), {} left in queue",
				rank, task.start, task.end, task_queue_.size());

			gd->MPI_ChangeDataRange(task.start, task.end);
			MultiRunner::MultiRun(task.start);
			// 结果自动累积到 PerformanceCollector
		}
		// =========================================
		// 6. 无任务 → 尝试窃取
		// =========================================
		else
		{
			// 6a. 检查是否有等待中的窃取响应（可能填充 task_queue_）
			check_steal_response();

			// 6b. 若没有进行中的窃取且队列仍为空，发起新的窃取
			if (!steal_pending_ && task_queue_.empty())
			{
				if (all_victims_known_empty())
				{
					report_done();
				}
				else
				{
					initiate_steal();
				}
			}
		}
	}
}

// ===================================================================
// 处理外来窃取请求（MPI_Iprobe 非阻塞探测）
// ===================================================================
void HostManager::service_incoming_steals()
{
	int flag = 0;
	MPI_Status status;

	while (true)
	{
		MPI_Iprobe(MPI_ANY_SOURCE, TAG_STEAL_REQ, MPI_COMM_WORLD, &flag, &status);
		if (!flag) break;

		int thief = status.MPI_SOURCE;
		int dummy;
		MPI_Recv(&dummy, 1, MPI_INT, thief, TAG_STEAL_REQ,
		         MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		// 决定给多少：ceil(queue_size / 2)，从前端取（FIFO = 最老的任务）
		int give = (static_cast<int>(task_queue_.size()) + 1) / 2;

		LOG_INFO("MPI rank {}: steal request from rank {}, giving {} tasks (have {})",
			rank, thief, give, task_queue_.size());

		// 先发 count
		MPI_Send(&give, 1, MPI_INT, thief, TAG_STEAL_RESP, MPI_COMM_WORLD);

		// 再发任务（从前端取出，批量给）
		if (give > 0)
		{
			std::vector<TaskRange> batch;
			for (int i = 0; i < give; ++i)
			{
				batch.push_back(task_queue_.front());
				task_queue_.pop_front();
			}
			MPI_Send(batch.data(), give, *mpi_task_range_type,
			         thief, TAG_STEAL_RESP, MPI_COMM_WORLD);
		}
	}
}

// ===================================================================
// 发起窃取：随机选 victim，非阻塞发送请求
// ===================================================================
void HostManager::initiate_steal()
{
	steal_target_ = pick_random_victim();
	if (steal_target_ < 0) return;  // 没有可偷的对象

	LOG_INFO("MPI rank {}: attempting to steal from rank {}", rank, steal_target_);

	int dummy = 0;
	MPI_Isend(&dummy, 1, MPI_INT, steal_target_, TAG_STEAL_REQ,
	          MPI_COMM_WORLD, &steal_req_);
	steal_pending_ = true;
}

// ===================================================================
// 检查窃取响应是否已到达（MPI_Iprobe + Recv）
// ===================================================================
void HostManager::check_steal_response()
{
	if (!steal_pending_) return;

	int flag = 0;
	MPI_Status status;
	MPI_Iprobe(steal_target_, TAG_STEAL_RESP, MPI_COMM_WORLD, &flag, &status);
	if (!flag) return;  // 还没到

	// 收 count
	int count = 0;
	MPI_Recv(&count, 1, MPI_INT, steal_target_, TAG_STEAL_RESP,
	         MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	if (count > 0)
	{
		// 收任务数据
		std::vector<TaskRange> batch(count);
		MPI_Recv(batch.data(), count, *mpi_task_range_type,
		         steal_target_, TAG_STEAL_RESP, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

		// 加入本地队列后端（LIFO 给自己用）
		for (auto& t : batch)
			task_queue_.push_back(t);

		LOG_INFO("MPI rank {}: stole {} tasks from rank {}, now have {}",
			rank, count, steal_target_, task_queue_.size());

		// 窃取成功 → 重置 victim 标记（被偷的 victim 可能还有剩余）
		victim_empty_[steal_target_] = false;
	}
	else
	{
		// count == 0：victim 也是空的
		// 注意：victim 之后可能从别处窃取到任务，但我们的标记不会自动失效。
		// 这是简化版终止检测的已知局限——最坏情况下本进程提前空闲，
		// 但 victim 最终会完成并报告 DONE，不影响全局终止的正确性。
		victim_empty_[steal_target_] = true;
		LOG_INFO("MPI rank {}: steal from rank {} returned empty", rank, steal_target_);
	}

	// 清理 Isend handle
	if (steal_req_ != MPI_REQUEST_NULL)
	{
		MPI_Wait(&steal_req_, MPI_STATUS_IGNORE);
		steal_req_ = MPI_REQUEST_NULL;
	}

	steal_pending_ = false;
	steal_target_  = -1;
}

// ===================================================================
// 随机选择一个尚未被证实为空的 victim（排除自己和已知空的）
// ===================================================================
int HostManager::pick_random_victim()
{
	std::vector<int> candidates;
	for (int i = 0; i < rank_size; ++i)
	{
		if (i != rank && !victim_empty_[i])
			candidates.push_back(i);
	}
	if (candidates.empty()) return -1;

	// 用简单的 rand 选（已用 rank 做种子则足够分散）
	int idx = std::rand() % static_cast<int>(candidates.size());
	return candidates[idx];
}

// ===================================================================
// 是否所有 victim 都已证实为空
// ===================================================================
bool HostManager::all_victims_known_empty() const
{
	for (int i = 0; i < rank_size; ++i)
		if (i != rank && !victim_empty_[i])
			return false;
	return true;
}

// ===================================================================
// 向 rank 0 报告完成
// ===================================================================
void HostManager::report_done()
{
	LOG_INFO("MPI rank {}: all victims empty + local queue empty, reporting DONE", rank);

	if (rank != 0)
	{
		int dummy = 0;
		MPI_Send(&dummy, 1, MPI_INT, 0, TAG_DONE, MPI_COMM_WORLD);
	}
	i_am_done_ = true;
}

// ===================================================================
// 非阻塞检查是否收到 rank 0 的终止广播
// ===================================================================
bool HostManager::poll_termination()
{
	if (rank == 0) return false;  // rank 0 自己是终止的发起者

	int flag = 0;
	MPI_Status status;
	MPI_Iprobe(0, TAG_TERM, MPI_COMM_WORLD, &flag, &status);
	if (flag)
	{
		int dummy;
		MPI_Recv(&dummy, 0, MPI_INT, 0, TAG_TERM,
		         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		globally_done_ = true;
		return true;
	}
	return false;
}

// ===================================================================
// Phase 2 — MPI_Gatherv 收集所有进程的本地累积结果到 rank 0
// ===================================================================
void HostManager::gather_results()
{
	std::vector<BacktestSummary> local_results =
		PerformanceCollector::GetPerformanceCollector()->TakeAllSummaries();

	int local_count = static_cast<int>(local_results.size());

	LOG_INFO("MPI rank {}: entering gather with {} local summaries", rank, local_count);

	// Step 1: 收集各进程的 summary 数量到 root
	std::vector<int> all_counts(rank_size);
	MPI_Gather(&local_count, 1, MPI_INT,
	           all_counts.data(), 1, MPI_INT,
	           0, MPI_COMM_WORLD);

	// Step 2 & 3: Root 分配缓冲区并调用 MPI_Gatherv
	if (rank == 0)
	{
		std::vector<int> displs(rank_size);
		int total = 0;
		for (int i = 0; i < rank_size; ++i)
		{
			displs[i] = total;
			total += all_counts[i];
		}

		std::vector<BacktestSummary> all_results(total);
		LOG_INFO("MPI rank 0: expecting {} total summaries", total);

		MPI_Gatherv(local_results.data(), local_count, *mpi_summary_type,
		            all_results.data(), all_counts.data(), displs.data(), *mpi_summary_type,
		            0, MPI_COMM_WORLD);

		PerformanceCollector::GetPerformanceCollector()->LoadAllSummaries(all_results);
		LOG_INFO("MPI rank 0: gathered {} total summaries via MPI_Gatherv", all_results.size());
	}
	else
	{
		MPI_Gatherv(local_results.data(), local_count, *mpi_summary_type,
		            nullptr, nullptr, nullptr, *mpi_summary_type,
		            0, MPI_COMM_WORLD);
	}
}
