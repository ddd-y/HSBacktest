// HSBacktest.h: 系统初始化 & 顶层入口

#pragma once

// ==========================================
// 初始化整个回测系统并启动运行
//
// 配置从 Configer 读取（stock_data_files / use_mpi / log_path）
// 单机模式 → MultiRunner::MultiRun()
// MPI 模式  → HostManager::distribute_task()
//
// @return 成功返回 true
// ==========================================
bool InitAndRun();
