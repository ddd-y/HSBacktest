#pragma once
struct BacktestSummary {
	double total_return = 0.0;           // 总收益率
	double annual_return = 0.0;          // 年化收益率
	double annual_volatility = 0.0;      // 年化波动率
	double sharpe_ratio = 0.0;           // 夏普比率
	double max_drawdown = 0.0;           // 最大回撤
	double avg_turnover = 0.0;           // 平均换手率
	int param_index = 0;                 // 对应的 adjust_params 索引
	BacktestSummary() = default;
	BacktestSummary(const BacktestSummary&) = default;
};