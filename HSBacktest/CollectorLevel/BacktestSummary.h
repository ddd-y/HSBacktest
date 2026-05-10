#pragma once
struct BacktestSummary {
	double total_return = 0.0;           // 总收益率
	double annual_return = 0.0;          // 年化收益率
	double annual_volatility = 0.0;      // 年化波动率
	double sharpe_ratio = 0.0;           // 夏普比率
	double max_drawdown = 0.0;           // 最大回撤
	double win_rate = 0.0;               // 胜率（日收益为正的天数占比）
	int total_trade_days = 0;            // 总交易天数
	int total_rebalances = 0;            // 总调仓次数
	double avg_turnover = 0.0;           // 平均换手率
	double final_net_value = 0.0;        // 最终净值
	double initial_capital = 0.0;        // 初始资金
	BacktestSummary() = default;
	BacktestSummary(const BacktestSummary&) = default;
};