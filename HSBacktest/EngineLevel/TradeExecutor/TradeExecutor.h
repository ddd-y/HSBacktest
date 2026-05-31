#pragma once
#include <vector>
#include <string>
#include <unordered_map>

// ==========================================
// 持仓记录结构体
// ==========================================
struct PositionRecord {
	int stock_index = -1;       // 在GlobalData中的股票索引
	int shares = 0;             // 持有股数
	double avg_cost = 0.0;      // 平均持仓成本（含交易费用分摊）
	double current_value = 0.0; // 当前市值
	double pnl = 0.0;           // 累计盈亏
};

// ==========================================
// 交易记录结构体
// ==========================================
struct TradeRecord {
	int trade_date = 0;         // 交易日期 (YYYYMMDD)
	int stock_index = -1;       // 股票索引
	bool is_buy;                // true=买入, false=卖出
	int shares;                 // 成交股数
	double price;               // 成交价格
	double commission;          // 佣金
	double stamp_duty;          // 印花税
	double transfer_fee;        // 过户费
	double slippage;            // 滑点成本
	double total_cost;          // 总交易成本
};

// ==========================================
// 净值快照（每个交易日记录一次）
// ==========================================
struct NetValueSnapshot {
	int trade_date = 0;
	double total_net_value = 0.0;    // 总净值（持仓市值 + 现金）
	double market_value = 0.0;       // 持仓市值
	double cash = 0.0;               // 可用资金
	double daily_return = 0.0;       // 日收益率
	double cumulative_return = 0.0;  // 累计收益率
};

// ==========================================
// TradeDataManager：管理账户状态和持仓数据
// 职责单一：状态存储与查询，不含交易执行逻辑
// ==========================================
class TradeDataManager {
	friend class TradeExecutor;
private:
	// ===== 账户核心状态 =====
	double available_capital = 0.0;        // 可用资金
	double total_net_value = 0.0;          // 总净值（市值 + 现金）
	double initial_capital = 0.0;          // 初始资金（用于收益率计算）

	// ===== 持仓与记录 =====
	std::vector<PositionRecord> positions;              // 当前持仓列表
	std::vector<TradeRecord> trade_history;             // 交易历史
	std::vector<NetValueSnapshot> nav_history;          // 净值历史（逐日）

public:
	explicit TradeDataManager(double init_capital = 1000000.0);

	// ===== 查询接口 =====
	double GetAvailableCapital() const { return available_capital; }
	double GetTotalNetValue() const { return total_net_value; }
	double GetInitialCapital() const { return initial_capital; }
	double GetTotalReturn() const;
	int GetPositionCount() const { return static_cast<int>(positions.size()); }

	// 查询某只股票是否在持仓中
	bool IsHolding(int stock_index) const;

	// 查询某只股票的持仓记录
	const PositionRecord* GetPosition(int stock_index) const;

	// 查询所有持仓
	const std::vector<PositionRecord>& GetAllPositions() const { return positions; }

	// 查询交易历史
	const std::vector<TradeRecord>& GetTradeHistory() const { return trade_history; }

	// 查询净值历史
	const std::vector<NetValueSnapshot>& GetNavHistory() const { return nav_history; }
};

// ==========================================
// TradeExecutor：交易执行器
// 职责：执行买卖、风控检查、交易成本计算、逐日盯市
// ==========================================
class TradeExecutor {
private:
	TradeDataManager data_manager;

	// ===== 交易成本计算 =====
	struct TransactionCostResult {
		double commission;
		double stamp_duty;
		double transfer_fee;
		double slippage;
		double total;
	};
	TransactionCostResult CalcTransactionCost(double price, int shares, bool is_buy) const;

	// ===== 风控检查 =====
	struct RiskCheckResult {
		bool passed = true;
		std::string reason;
	};
	RiskCheckResult CheckStopLoss(const PositionRecord& pos, double current_price) const;
	RiskCheckResult CheckTakeProfit(const PositionRecord& pos, double current_price) const;

	// 辅助：重新计算总净值 = 现金 + 所有持仓市值
	void RecalcNetValue();

	// 辅助：记录一笔交易到 trade_history
	void RecordTrade(int trade_date, int stock_index, bool is_buy, int shares, double price,
		const TransactionCostResult& cost);

public:
	explicit TradeExecutor(double init_capital = 1000000.0);

	// ===== 主接口 =====
	// 执行买入：返回实际成交股数
	int ExecuteBuy(int stock_index, double price, int target_shares, int trade_date);

	// 执行卖出：返回实际成交股数
	int ExecuteSell(int stock_index, double price, int target_shares, int trade_date);

	// 全仓卖出指定股票
	void ClosePosition(int stock_index, double price, int trade_date);

	// 平掉所有持仓
	void CloseAllPositions(const std::vector<int>& trade_dates, const std::vector<double>& prices);

	// ===== 逐日更新 =====
	// 每日更新持仓市值（根据当日收盘价）
	void UpdateMarketValue(int stock_index, double close_price);

	// 更新所有持仓市值的日终值
	void UpdateAllMarketValues(const std::vector<double>& close_prices);

	// 记录当日净值快照
	void RecordDailySnapshot(int trade_date);

	// 处理公司行为（分红、拆股），更新持仓和可用资金
	// @param stock_index 股票索引
	// @param date_abs_idx 在 GlobalData dates 中的绝对索引
	void ProcessCorporateActions(int stock_index, int date_abs_idx);

	// 批量处理所有持仓的公司行为
	// @param date_abs_idx 在 GlobalData dates 中的绝对索引
	void ProcessAllCorporateActions(int date_abs_idx);

	// 检查所有持仓是否需要止损/止盈
	void CheckAllStopLossTakeProfit(const std::vector<double>& current_prices, int trade_date);

	// ===== 调仓接口 =====
	// 调仓：卖出不在目标列表中的股票，买入目标列表中的股票
	// target_indices: 目标持仓股票索引列表
	// prices: 所有股票的当前价格
	// trade_date: 当前交易日期
	void Rebalance(const std::vector<int>& target_indices,
		const std::vector<double>& prices,
		int trade_date,
		double total_capital);

	// ===== 查询 =====
	const TradeDataManager& GetDataManager() const { return data_manager; }
	TradeDataManager& GetDataManager() { return data_manager; }

	// ===== 初始化 =====
	void SetInitialCapital(double capital);

	// 重新初始化：重置所有状态（清仓、清交易记录、清净值历史、恢复初始资金）
	// @param init_capital 新的初始资金
	void ReInitialize(double init_capital = 1000000.0);
};
