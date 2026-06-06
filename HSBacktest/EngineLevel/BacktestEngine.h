#pragma once
#include <vector>
#include <string>
#include <memory>
#include"../CollectorLevel/BacktestSummary.h"
#include "TradeExecutor/TradeExecutor.h"

class TradeExecutor;
class StockSelector;


// ==========================================
// BacktestEngine：回测引擎主控
// 职责：
// 1. 生命周期管理（初始化 → 运行 → 报告）
// 2. 回测主循环编排（逐日推进）
// 3. 协调 StockSelector 与 TradeExecutor
// 4. 绩效指标计算
// ==========================================
class BacktestEngine {
private:
	// ===== 核心组件（拥有） =====
	TradeExecutor* trade_executor = nullptr;
	StockSelector* stock_selector = nullptr;

	// ===== 回测状态 =====
	bool is_initialized = false;
	double initial_capital = 1000000.0;


	//并行跑多个参数的回测时，对应的调整参数数组索引（同时用于权重和top_n）
	int adjustParamIndex = 0;
public:
	BacktestEngine();
	~BacktestEngine();

	// ===== 初始化 =====
	// @param init_capital 初始资金
	// @param adjust_param_index 调整参数索引（同时用于权重和选股数量）
	void Initialize(double init_capital = 1000000.0, int adjust_param_index = 0);

	//  重新初始化
	// @param init_capital 初始资金
	// @param adjust_param_index 调整参数索引
	void ReInitialize(double init_capital = 1000000.0, int adjust_param_index = 0);

	bool IsInitialized() const { return is_initialized; }

	// ===== 运行回测 =====
	// 执行完整的回测循环
	void Run();

	// ===== 结果查询 =====
	BacktestSummary GetSummary() const { return summary; }
	const TradeExecutor* GetTradeExecutor() const { return trade_executor; }

	// ===== 绩效报告 =====
	// 计算绩效指标
	void CalculatePerformance();

	// 打印绩效报告到日志和标准输出
	void PrintReport() const;

	// 导出净值曲线到CSV
	void ExportNavToCsv(const std::string& filename) const;

	// 导出交易记录到CSV
	void ExportTradesToCsv(const std::string& filename) const;

private:
	// ===== 内部辅助 =====
	BacktestSummary summary;

	// 获取所有股票在指定日期的收盘价
	std::vector<double> GetAllStockClosePrices(int date_absolute_idx) const;

	// 计算最大回撤
	double CalculateMaxDrawdown(const std::vector<NetValueSnapshot>& nav_history) const;

	// 逐日推进：处理公司行为 → 更新市值 → 止损止盈 → 记录快照
	void ProcessDailyLoop(int from_idx, int to_idx);
};
