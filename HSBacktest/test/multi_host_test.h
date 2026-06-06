#pragma once
// ==========================================
// multi_host_test.h — HostManager 单元测试（不依赖 MPI）
//
// 运行: #include "test/multi_host_test.h"
//        run_multi_host_unit_tests();
//
// 端到端 MPI 测试见 multi_host_e2e_test.h
// ==========================================

#include "../ParaLevel/multi_host/multi_host.h"
#include "../Datalevel/Global_data.h"
#include "../CollectorLevel/performance_collector.h"
#include "../CollectorLevel/BacktestSummary.h"
#include "../ConfigLvevl/configer.h"
#include "../MyLog/Logger.h"
#include <algorithm>
#include <numeric>
#include <set>

#define MH_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            LOG_ERROR("[MH-FAIL] {} ({}:{})", msg, __FILE__, __LINE__); \
            mh_all_passed = false; \
        } else { \
            LOG_INFO("[MH-PASS] {}", msg); \
        } \
    } while(0)

static bool mh_all_passed = true;

void test_task_partition()
{
    LOG_INFO("=== [A] 任务分区测试 ===");
    {
        HostManager mgr;
        mgr.SetRankAndSizeForTest(0, 3);
        mgr.BuildInitialTasksForTest(500);
        MH_CHECK(mgr.GetTaskQueue().size() == 1, "3ranks×500: rank0得1");
        MH_CHECK(mgr.GetVictimEmpty().size() == 3, "victim_empty_=3");
        MH_CHECK(mgr.GetVictimEmpty()[0] == true, "rank0标记自己空");
    }
    {
        HostManager mgr;
        mgr.SetRankAndSizeForTest(1, 3);
        mgr.BuildInitialTasksForTest(500);
        MH_CHECK(mgr.GetTaskQueue().size() == 0, "3ranks×500: rank1空");
    }
    {
        HostManager mgr;
        mgr.SetRankAndSizeForTest(2, 4);
        mgr.BuildInitialTasksForTest(2048);
        MH_CHECK(mgr.GetTaskQueue().size() == 1, "4ranks×2048: 每人1");
        MH_CHECK(mgr.GetTaskQueue()[0].start == 1024, "rank2 task start=1024");
        MH_CHECK(mgr.GetTaskQueue()[0].end == 1536, "rank2 task end=1536");
    }
    {
        HostManager a, b;
        a.SetRankAndSizeForTest(0, 2); b.SetRankAndSizeForTest(1, 2);
        a.BuildInitialTasksForTest(1536); b.BuildInitialTasksForTest(1536);
        MH_CHECK(a.GetTaskQueue().size() == 2, "2ranks×1536: rank0得2");
        MH_CHECK(b.GetTaskQueue().size() == 1, "2ranks×1536: rank1得1");
        std::set<int> cov;
        for (auto& t : a.GetTaskQueue()) for (int i=t.start;i<t.end;++i) cov.insert(i);
        for (auto& t : b.GetTaskQueue()) for (int i=t.start;i<t.end;++i) cov.insert(i);
        MH_CHECK(cov.size() == 1536, "全覆盖无重复 (1536个)");
    }
}

void test_victim_selection()
{
    LOG_INFO("=== [B] victim 选择 ===");
    {
        HostManager mgr;
        mgr.SetRankAndSizeForTest(0, 4); mgr.BuildInitialTasksForTest(2048);
        int p = mgr.PickRandomVictimForTest();
        MH_CHECK(p>=1 && p<=3, "候选={1,2,3}");
        MH_CHECK(!mgr.AllVictimsKnownEmptyForTest(), "未全部为空");
    }
    {
        HostManager mgr;
        mgr.SetRankAndSizeForTest(2, 4); mgr.BuildInitialTasksForTest(2048);
        int p = mgr.PickRandomVictimForTest();
        MH_CHECK(p!=2, "不选自己");
    }
}

void test_termination_detection()
{
    LOG_INFO("=== [C] 终止检测 ===");
    {
        HostManager mgr;
        mgr.SetRankAndSizeForTest(0, 2); mgr.BuildInitialTasksForTest(2048);
        MH_CHECK(!mgr.IsDone(), "有任务非done");
        MH_CHECK(!mgr.IsGloballyDone(), "非全局终止");
    }
    {
        HostManager mgr;
        mgr.SetRankAndSizeForTest(0, 2); mgr.BuildInitialTasksForTest(500);
        MH_CHECK(!mgr.AllVictimsKnownEmptyForTest(), "rank1未证实");
    }
}

inline void run_multi_host_unit_tests()
{
    HSBacktest::Logger::getInstance().init("test_log.txt");
    LOG_INFO("========== HostManager 单元测试 ==========");
    test_task_partition();
    test_victim_selection();
    test_termination_detection();
    if (mh_all_passed) LOG_INFO("========== 全部通过! ==========");
    else              LOG_ERROR("========== 存在失败项! ==========");
}
