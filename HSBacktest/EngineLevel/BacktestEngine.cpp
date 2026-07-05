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

BacktestEngine::BacktestEngine() = default;

BacktestEngine::~BacktestEngine() = default;

void BacktestEngine::Initialize(double init_capital, int adjust_param_index)
{
	adjustParamIndex = adjust_param_index;
	initial_capital = init_capital;

	// 创建核心组件
	trade_executor = std::make_unique<TradeExecutor>(init_capital);
	stock_selector = std::make_unique<StockSelector>(adjustParamIndex);

	// 读取策略参数
	const auto& strategy_cfg = Configer::GetStrategyConfiger();

	// top_n 通过 adjustParamIndex 从 GlobalData 获取，不再从 StrategyConfiger 读取
	int top_n = GlobalData::GetGlobalData() ? GlobalData::GetGlobalData()->GetTopN(adjustParamIndex) : strategy_cfg.GetTopN();

	is_initialized = true;

	LOG_DEBUG("BacktestEngine initialized | init_capital={:.2f} |  top_n={} | adjustParamIndex={}",
		init_capital, top_n, adjustParamIndex);
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
	summary.param_index = adjustParamIndex;

	int top_n = GlobalData::GetGlobalData() ? GlobalData::GetGlobalData()->GetTopN(adjustParamIndex) : 50;

	LOG_DEBUG("BacktestEngine reinitialized | init_capital={:.2f} | top_n={} | adjustParamIndex={}",
		init_capital, top_n, adjustParamIndex);
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



	LOG_DEBUG("BacktestEngine::Run - starting backtest with {} stocks, {} rebalance periods",
		stock_count, rebalance_indices.size());

	// === 主循环：遍历每个调仓日 ===
	for (int rb_idx = 0; rb_idx < static_cast<int>(rebalance_indices.size()); ++rb_idx) {
		int date_abs_idx = rebalance_indices[rb_idx];

		// 调仓日当天先处理公司行为（分红/拆股），确保持仓状态正确
		trade_executor->ProcessAllCorporateActions(date_abs_idx);

		std::vector<double> close_prices = GetAllStockClosePrices(date_abs_idx);

		int32_t trade_date = gd->get_dates()[date_abs_idx];
		LOG_DEBUG("=== Rebalance #{} | date={} | date_idx={} ===",
			rb_idx, trade_date, date_abs_idx);

		// 选股 + 调仓（失败不影响逐日处理）
		std::vector<int> selected_stocks;
		try {
			selected_stocks = stock_selector->ScoreAndSelect(rb_idx);
		}
		catch (const std::exception& e) {
			LOG_ERROR("StockSelector failed at rebalance #{}: {}", rb_idx, e.what());
		}

		if (!selected_stocks.empty()) {
			try {
				trade_executor->Rebalance(selected_stocks, close_prices, trade_date);
			}
			catch (const std::exception& e) {
				LOG_ERROR("TradeExecutor::Rebalance failed at rebalance #{}: {}", rb_idx, e.what());
			}
		}
		else {
			LOG_DEBUG("No stocks selected at rebalance #{}", rb_idx);
		}

		// 记录调仓日净值快照（无论选股/调仓是否成功）
		trade_executor->RecordDailySnapshot(trade_date);

		// 无论调仓是否成功，必须处理逐日盯市
		int next_date_abs_idx = (rb_idx + 1 < static_cast<int>(rebalance_indices.size()))
			? rebalance_indices[rb_idx + 1]
			: static_cast<int>(gd->get_dates().size()-1);

		if (next_date_abs_idx > date_abs_idx) {
			ProcessDailyLoop(date_abs_idx + 1, next_date_abs_idx);
		}
	}

	const auto& dates = gd->get_dates();
	int last_idx = static_cast<int>(dates.size()) - 1;
	std::vector<double> final_prices = GetAllStockClosePrices(last_idx);

	// 最后一天逐日处理（公司行为 / 市值更新 / 止损止盈），清仓前必须做
	trade_executor->ProcessAllCorporateActions(last_idx);
	trade_executor->UpdateAllMarketValues(final_prices);
	trade_executor->CheckAllStopLossTakeProfit(final_prices, dates[last_idx]);

	trade_executor->CloseAllPositions(dates[last_idx], final_prices);
	trade_executor->RecordDailySnapshot(dates[last_idx]);

	CalculatePerformance();
	//PrintReport();

	LOG_DEBUG("BacktestEngine::Run - backtest completed");
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
			prices[i] = daily_datas[date_absolute_idx].close;
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

	// 基础数据（最终净值从 TradeExecutor 实时读取，反映清仓后的真实状态）
	double final_net_value = trade_executor->GetDataManager().GetTotalNetValue();
	double total_return = (final_net_value - initial_capital) / initial_capital;
	summary.total_return = total_return;
	int total_trade_days = static_cast<int>(nav_history.size());

	// 计算年化收益率 (交易日=252)
	// 仅在总收益 > -1.0 时年化，否则直接用累计收益
	double years = static_cast<double>(total_trade_days) / ANNUAL_TRADE_DAYS;
	if (years > 0.0 && total_return > -1.0) {
		summary.annual_return = std::pow(1.0 + total_return, 1.0 / years) - 1.0;
	}
	else {
		summary.annual_return = total_return;
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

	}

	// 最大回撤
	summary.max_drawdown = CalculateMaxDrawdown(nav_history);

	// 平均换手率
	const auto& turnover_rates = trade_executor->GetDataManager().GetTurnoverRates();
	if (!turnover_rates.empty()) {
		summary.avg_turnover = std::accumulate(turnover_rates.begin(), turnover_rates.end(), 0.0)
			/ turnover_rates.size();
	}
}

void BacktestEngine::PrintReport() const
{
	LOG_DEBUG("========================================");
	LOG_DEBUG("========== Backtest Report =============");
	LOG_DEBUG("========================================");
	LOG_DEBUG("Total Return:        {:>12.4f}%", summary.total_return * 100.0);
	LOG_DEBUG("Annual Return:       {:>12.4f}%", summary.annual_return * 100.0);
	LOG_DEBUG("Annual Volatility:   {:>12.4f}%", summary.annual_volatility * 100.0);
	LOG_DEBUG("Sharpe Ratio:        {:>12.4f}", summary.sharpe_ratio);
	LOG_DEBUG("Max Drawdown:        {:>12.4f}%", summary.max_drawdown * 100.0);
	LOG_DEBUG("Avg Turnover:        {:>12.4f}%", summary.avg_turnover * 100.0);
	LOG_DEBUG("Param Index:         {:>12d}", summary.param_index);
	LOG_DEBUG("========================================");

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

	LOG_DEBUG("BacktestEngine::ExportNavToCsv - exported {} records to {}", nav_history.size(), filename);
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

	ofs << "trade_date,stock_code,side,shares,price,commission,stamp_duty,transfer_fee,slippage,total_cost" << std::endl;
	for (const auto& trade : trade_history) {
		const std::string tstock_code = GlobalData::GetGlobalData()->get_stock_k_data(trade.stock_index)->GetStockCode();
		ofs << trade.trade_date << ","
			<< tstock_code << ","
			<< (trade.is_buy ? "BUY" : "SELL") << ","
			<< trade.shares << ","
			<< std::fixed << std::setprecision(2) << trade.price << ","
			<< std::setprecision(4) << trade.commission << ","
			<< trade.stamp_duty << ","
			<< trade.transfer_fee << ","
			<< trade.slippage << ","
			<< trade.total_cost << std::endl;
	}

	LOG_DEBUG("BacktestEngine::ExportTradesToCsv - exported {} records to {}", trade_history.size(), filename);
}
