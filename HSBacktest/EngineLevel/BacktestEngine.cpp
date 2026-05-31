#include "BacktestEngine.h"
#include "TradeExecutor/TradeExecutor.h"
#include "StockSelector/StockSelector.h"
#include "../Datalevel/Global_data.h"
#include "../Datalevel/stock_k_data.h"
#include "../Datalevel/data_defs.h"
#include "../ConfigLvevl/configer.h"
#include "../MyLog/Logger.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <fstream>
#include <iomanip>

BacktestEngine::BacktestEngine()
	: trade_executor(nullptr)
	, stock_selector(nullptr)
{
}

BacktestEngine::~BacktestEngine()
{
	delete trade_executor;
	trade_executor = nullptr;
	delete stock_selector;
	stock_selector = nullptr;
}

void BacktestEngine::Initialize(double init_capital, int adjust_param_index)
{
	adjustParamIndex = adjust_param_index;
	initial_capital = init_capital;

	// 创建核心组件
	trade_executor = new TradeExecutor(init_capital);
	stock_selector = new StockSelector(adjustParamIndex);

	// 读取策略参数
	const auto& strategy_cfg = Configer::GetStrategyConfiger();
	hold_days = strategy_cfg.GetHoldDays();

	// top_n 通过 adjustParamIndex 从 GlobalData 获取，不再从 StrategyConfiger 读取
	int top_n = GlobalData::GetGlobalData() ? GlobalData::GetGlobalData()->GetTopN(adjustParamIndex) : strategy_cfg.GetTopN();

	is_initialized = true;

	LOG_INFO("BacktestEngine initialized | init_capital={:.2f} | hold_days={} | top_n={} | adjustParamIndex={}",
		init_capital, hold_days, top_n, adjustParamIndex);
}

void BacktestEngine::ReInitialize(double init_capital, int adjust_param_index)
{
	adjustParamIndex = adjust_param_index;
	initial_capital = init_capital;

	// 重置 TradeExecutor 状态（清仓、清记录、恢复资金）
	trade_executor->ReInitialize(init_capital);

	// 切换参数索引（同时更新权重和选股数量）
	stock_selector->ReSetAdjustParamIndex(adjustParamIndex);

	// 重置summary
	summary	= BacktestSummary();

	current_date_idx = 0;
	current_rebalance_idx = 0;

	int top_n = GlobalData::GetGlobalData() ? GlobalData::GetGlobalData()->GetTopN(adjustParamIndex) : 50;

	LOG_INFO("BacktestEngine reinitialized | init_capital={:.2f} | hold_days={} | top_n={} | adjustParamIndex={}",
		init_capital, hold_days, top_n, adjustParamIndex);
}


void BacktestEngine::Run()
{
	GlobalData* gd = GlobalData::GetGlobalData();
	if (!is_initialized || !gd) {
		LOG_ERROR("BacktestEngine::Run - engine not initialized");
		return;
	}

	int stock_count = gd->get_stock_count();
	if (stock_count <= 0) {
		LOG_ERROR("BacktestEngine::Run - no stock data available");
		return;
	}

	const auto& rebalance_indices = gd->get_rebalance_index();
	if (rebalance_indices.empty()) {
		LOG_ERROR("BacktestEngine::Run - no rebalance dates available");
		return;
	}

	summary.total_rebalances = static_cast<int>(rebalance_indices.size());
	summary.initial_capital = initial_capital;

	LOG_INFO("BacktestEngine::Run - starting backtest with {} stocks, {} rebalance periods",
		stock_count, rebalance_indices.size());

	// 创建第一个调仓日的净值快照
	trade_executor->RecordDailySnapshot(gd->get_dates()[rebalance_indices[0]]);

	// === 主循环：遍历每个调仓日 ===
	for (int rb_idx = 0; rb_idx < static_cast<int>(rebalance_indices.size()); ++rb_idx) {
		int date_abs_idx = rebalance_indices[rb_idx];
		current_rebalance_idx = rb_idx;

		// 调仓日当天先处理公司行为（分红/拆股），确保持仓状态正确
		trade_executor->ProcessAllCorporateActions(date_abs_idx);

		std::vector<double> close_prices = GetAllStockClosePrices(date_abs_idx);

		int32_t trade_date = gd->get_dates()[date_abs_idx];
		LOG_INFO("=== Rebalance #{} | date={} | date_idx={} ===",
			rb_idx, trade_date, date_abs_idx);

		std::vector<int> selected_stocks;
		try {
			selected_stocks = stock_selector->ScoreAndSelect(rb_idx);
		}
		catch (const std::exception& e) {
			LOG_ERROR("StockSelector failed at rebalance #{}: {}", rb_idx, e.what());
			trade_executor->RecordDailySnapshot(trade_date);
			continue;
		}

		if (selected_stocks.empty()) {
			LOG_WARN("No stocks selected at rebalance #{}", rb_idx);
			trade_executor->RecordDailySnapshot(trade_date);
			continue;
		}

		try {
			trade_executor->Rebalance(selected_stocks, close_prices, trade_date, initial_capital);
		}
		catch (const std::exception& e) {
			LOG_ERROR("TradeExecutor::Rebalance failed at rebalance #{}: {}", rb_idx, e.what());
			trade_executor->RecordDailySnapshot(trade_date);
			continue;
		}

		int next_date_abs_idx = (rb_idx + 1 < static_cast<int>(rebalance_indices.size()))
			? rebalance_indices[rb_idx + 1]
			: -1;

		if (next_date_abs_idx > date_abs_idx) {
			ProcessDailyLoop(date_abs_idx + 1, next_date_abs_idx);
		}
	}

	const auto& dates = gd->get_dates();
	int last_idx = static_cast<int>(dates.size()) - 1;
	std::vector<double> final_prices = GetAllStockClosePrices(last_idx);
	trade_executor->CloseAllPositions(
		std::vector<int>(final_prices.size(), dates[last_idx]),
		final_prices
	);
	trade_executor->RecordDailySnapshot(dates[last_idx]);

	CalculatePerformance();
	PrintReport();

	LOG_INFO("BacktestEngine::Run - backtest completed");
}

bool BacktestEngine::StepRebalance()
{
	GlobalData* gd = GlobalData::GetGlobalData();
	if (!is_initialized || !gd) return false;

	const auto& rebalance_indices = gd->get_rebalance_index();
	if (current_rebalance_idx >= static_cast<int>(rebalance_indices.size())) return false;

	int date_abs_idx = rebalance_indices[current_rebalance_idx];

	// 调仓日当天先处理公司行为
	trade_executor->ProcessAllCorporateActions(date_abs_idx);

	std::vector<double> close_prices = GetAllStockClosePrices(date_abs_idx);
	if (close_prices.empty()) {
		current_rebalance_idx++;
		return current_rebalance_idx < static_cast<int>(rebalance_indices.size());
	}

	const auto& dates = gd->get_dates();
	int trade_date = dates[date_abs_idx];

	std::vector<int> selected = stock_selector->ScoreAndSelect(current_rebalance_idx);
	if (!selected.empty()) {
		trade_executor->Rebalance(selected, close_prices, trade_date, initial_capital);
	}

	int next_date_abs_idx = (current_rebalance_idx + 1 < static_cast<int>(rebalance_indices.size()))
		? rebalance_indices[current_rebalance_idx + 1]
		: -1;

	if (next_date_abs_idx > date_abs_idx) {
		ProcessDailyLoop(date_abs_idx + 1, next_date_abs_idx);
	}

	current_rebalance_idx++;
	return current_rebalance_idx < static_cast<int>(rebalance_indices.size());
}

void BacktestEngine::ProcessDailyLoop(int from_idx, int to_idx)
{
	GlobalData* gd = GlobalData::GetGlobalData();
	if (!gd || from_idx >= to_idx) return;

	const auto& dates = gd->get_dates();
	for (int d = from_idx; d < to_idx; ++d) {
		std::vector<double> day_prices = GetAllStockClosePrices(d);
		if (day_prices.empty()) continue;

		trade_executor->ProcessAllCorporateActions(d);
		trade_executor->UpdateAllMarketValues(day_prices);
		trade_executor->CheckAllStopLossTakeProfit(day_prices, dates[d]);
		trade_executor->RecordDailySnapshot(dates[d]);
	}
}

int BacktestEngine::GetCurrentDateAbsoluteIndex() const
{
	GlobalData* gd = GlobalData::GetGlobalData();
	if (!gd) return -1;
	const auto& rebalance_indices = gd->get_rebalance_index();
	if (current_rebalance_idx < static_cast<int>(rebalance_indices.size())) {
		return rebalance_indices[current_rebalance_idx];
	}
	return -1;
}

std::vector<double> BacktestEngine::GetAllStockClosePrices(int date_absolute_idx) const
{
	GlobalData* gd = GlobalData::GetGlobalData();
	if (!gd) return {};

	int stock_count = gd->get_stock_count();
	std::vector<double> prices(stock_count, 0.0);

	for (int i = 0; i < stock_count; ++i) {
		StockKData* stock_data = gd->get_stock_k_data(i);
		if (!stock_data) continue;

		const auto& daily_datas = stock_data->get_daily_datas();
		if (date_absolute_idx >= 0 && date_absolute_idx < static_cast<int>(daily_datas.size())) {
			prices[i] = daily_datas[date_absolute_idx].close * daily_datas[date_absolute_idx].adj_factor;
		}
	}

	return prices;
}

double BacktestEngine::CalculateMaxDrawdown(const std::vector<NetValueSnapshot>& nav_history) const
{
	if (nav_history.empty()) return 0.0;

	double peak = nav_history[0].total_net_value;
	double max_dd = 0.0;

	for (const auto& snap : nav_history) {
		if (snap.total_net_value > peak) {
			peak = snap.total_net_value;
		}
		double dd = (peak - snap.total_net_value) / peak;
		if (dd > max_dd) {
			max_dd = dd;
		}
	}

	return max_dd;
}

void BacktestEngine::CalculatePerformance()
{
	const auto& nav_history = trade_executor->GetDataManager().GetNavHistory();
	if (nav_history.empty()) return;

	// 基础数据
	summary.final_net_value = nav_history.back().total_net_value;
	summary.total_return = (summary.final_net_value - initial_capital) / initial_capital;
	summary.total_trade_days = static_cast<int>(nav_history.size());

	// 计算年化收益率 (交易日=252)
	double years = static_cast<double>(summary.total_trade_days) / ANNUAL_TRADE_DAYS;
	if (years > 0.0) {
		summary.annual_return = std::pow(1.0 + summary.total_return, 1.0 / years) - 1.0;
	}
	else {
		summary.annual_return = summary.total_return;
	}

	// 计算年化波动率和夏普比率
	if (nav_history.size() >= 2) {
		std::vector<double> daily_returns;
		daily_returns.reserve(nav_history.size() - 1);

		for (size_t i = 1; i < nav_history.size(); ++i) {
			double prev_nav = nav_history[i - 1].total_net_value;
			if (prev_nav > 0.0) {
				daily_returns.push_back((nav_history[i].total_net_value - prev_nav) / prev_nav);
			}
		}

		// nav_history.size()>=2 保证了 daily_returns 至少有一个元素
		double mean_daily_ret = std::accumulate(daily_returns.begin(), daily_returns.end(), 0.0)
			/ daily_returns.size();

		double sq_sum = 0.0;
		int positive_days = 0;
		for (double ret : daily_returns) {
			sq_sum += (ret - mean_daily_ret) * (ret - mean_daily_ret);
			if (ret > 0.0) positive_days++;
		}
		double daily_stddev = std::sqrt(sq_sum / daily_returns.size());

		summary.annual_volatility = daily_stddev * std::sqrt(ANNUAL_TRADE_DAYS);

		double risk_free_rate = Configer::GetStrategyConfiger().GetRiskFreeRate();
		double daily_rf = risk_free_rate / ANNUAL_TRADE_DAYS;
		double excess_return = mean_daily_ret - daily_rf;
		if (daily_stddev > 1e-10) {
			summary.sharpe_ratio = std::sqrt(ANNUAL_TRADE_DAYS) * excess_return / daily_stddev;
		}

		summary.win_rate = static_cast<double>(positive_days) / daily_returns.size();
	}

	// 最大回撤
	summary.max_drawdown = CalculateMaxDrawdown(nav_history);
}

void BacktestEngine::PrintReport() const
{
	LOG_INFO("========================================");
	LOG_INFO("========== Backtest Report =============");
	LOG_INFO("========================================");
	LOG_INFO("Initial Capital:     {:>12.2f}", summary.initial_capital);
	LOG_INFO("Final Net Value:     {:>12.2f}", summary.final_net_value);
	LOG_INFO("Total Return:        {:>12.4f}%", summary.total_return * 100.0);
	LOG_INFO("Annual Return:       {:>12.4f}%", summary.annual_return * 100.0);
	LOG_INFO("Annual Volatility:   {:>12.4f}%", summary.annual_volatility * 100.0);
	LOG_INFO("Sharpe Ratio:        {:>12.4f}", summary.sharpe_ratio);
	LOG_INFO("Max Drawdown:        {:>12.4f}%", summary.max_drawdown * 100.0);
	LOG_INFO("Win Rate:            {:>12.4f}%", summary.win_rate * 100.0);
	LOG_INFO("Total Trade Days:    {:>12d}", summary.total_trade_days);
	LOG_INFO("Total Rebalances:    {:>12d}", summary.total_rebalances);
	LOG_INFO("========================================");

}

void BacktestEngine::ExportNavToCsv(const std::string& filename) const
{
	if (!trade_executor) {
		LOG_ERROR("BacktestEngine::ExportNavToCsv - trade_executor is null");
		return;
	}

	const auto& nav_history = trade_executor->GetDataManager().GetNavHistory();
	if (nav_history.empty()) {
		LOG_WARN("BacktestEngine::ExportNavToCsv - no NAV history to export");
		return;
	}

	std::ofstream ofs(filename);
	if (!ofs.is_open()) {
		LOG_ERROR("BacktestEngine::ExportNavToCsv - cannot open file: {}", filename);
		return;
	}

	ofs << "trade_date,total_net_value,market_value,cash,daily_return,cumulative_return" << std::endl;
	for (const auto& snap : nav_history) {
		ofs << snap.trade_date << ","
			<< std::fixed << std::setprecision(2) << snap.total_net_value << ","
			<< snap.market_value << ","
			<< snap.cash << ","
			<< std::setprecision(6) << snap.daily_return << ","
			<< snap.cumulative_return << std::endl;
	}

	LOG_INFO("BacktestEngine::ExportNavToCsv - exported {} records to {}", nav_history.size(), filename);
}

void BacktestEngine::ExportTradesToCsv(const std::string& filename) const
{
	if (!trade_executor) {
		LOG_ERROR("BacktestEngine::ExportTradesToCsv - trade_executor is null");
		return;
	}

	const auto& trade_history = trade_executor->GetDataManager().GetTradeHistory();
	if (trade_history.empty()) {
		LOG_WARN("BacktestEngine::ExportTradesToCsv - no trade history to export");
		return;
	}

	std::ofstream ofs(filename);
	if (!ofs.is_open()) {
		LOG_ERROR("BacktestEngine::ExportTradesToCsv - cannot open file: {}", filename);
		return;
	}

	ofs << "trade_date,stock_index,side,shares,price,commission,stamp_duty,transfer_fee,slippage,total_cost" << std::endl;
	for (const auto& trade : trade_history) {
		ofs << trade.trade_date << ","
			<< trade.stock_index << ","
			<< (trade.is_buy ? "BUY" : "SELL") << ","
			<< trade.shares << ","
			<< std::fixed << std::setprecision(2) << trade.price << ","
			<< std::setprecision(4) << trade.commission << ","
			<< trade.stamp_duty << ","
			<< trade.transfer_fee << ","
			<< trade.slippage << ","
			<< trade.total_cost << std::endl;
	}

	LOG_INFO("BacktestEngine::ExportTradesToCsv - exported {} records to {}", trade_history.size(), filename);
}
