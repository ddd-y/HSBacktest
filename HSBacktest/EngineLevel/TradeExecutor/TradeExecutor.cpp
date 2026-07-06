#include "TradeExecutor.h"
#include "../../ConfigLvevl/configer.h"
#include "../../Datalevel/Global_data.h"
#include "../../Datalevel/stock_k_data.h"
#include "../../MyLog/Logger.h"
#include <algorithm>
#include <numeric>
#include <cmath>

// ===================== TradeDataManager 实现 =====================

TradeDataManager::TradeDataManager(double init_capital)
	: available_capital(init_capital)
	, total_net_value(init_capital)
	, initial_capital(init_capital)
{
}

double TradeDataManager::GetTotalReturn() const
{
	if (initial_capital <= 0.0) return 0.0;
	return (total_net_value - initial_capital) / initial_capital;
}

bool TradeDataManager::IsHolding(int stock_index) const
{
	return std::any_of(positions.begin(), positions.end(),
		[stock_index](const PositionRecord& pos) { return pos.stock_index == stock_index; });
}

const PositionRecord* TradeDataManager::GetPosition(int stock_index) const
{
	auto it = std::find_if(positions.begin(), positions.end(),
		[stock_index](const PositionRecord& pos) { return pos.stock_index == stock_index; });
	if (it != positions.end()) {
		return &(*it);
	}
	return nullptr;
}

// ===================== TradeExecutor 实现 =====================

TradeExecutor::TradeExecutor(double init_capital)
	: data_manager(init_capital)
{
}

TradeExecutor::TransactionCostResult TradeExecutor::CalcTransactionCost(double price, int shares, bool is_buy) const
{
	TransactionCostResult result = { 0.0, 0.0, 0.0, 0.0, 0.0 };
	const auto& cost_cfg = Configer::GetTransactionCostConfiger();

	double notional = price * shares;
	if (notional <= 0.0) return result;

	// 佣金：万分之三，最低5元
	result.commission = std::max(notional * cost_cfg.GetCommissionRate(), cost_cfg.GetMinCommission());

	// 印花税：仅卖出收取
	if (!is_buy) {
		result.stamp_duty = notional * cost_cfg.GetStampDutyRate();
	}

	// 过户费：十万分之二
	result.transfer_fee = notional * cost_cfg.GetTransferFeeRate();

	// 滑点
	double slippage_rate = is_buy ? cost_cfg.GetBuySlippageRate() : cost_cfg.GetSellSlippageRate();
	result.slippage = notional * slippage_rate;

	result.total = result.commission + result.stamp_duty + result.transfer_fee + result.slippage;

	return result;
}

TradeExecutor::RiskCheckResult TradeExecutor::CheckStopLoss(const PositionRecord& pos, double current_price) const
{
	RiskCheckResult result;
	if (pos.avg_cost <= 0.0) return result;

	double loss_ratio = (current_price - pos.avg_cost) / pos.avg_cost;
	if (loss_ratio < -Configer::GetStrategyConfiger().GetSingleStockStopLoss()) {
		result.passed = false;
		result.reason = "Stop loss triggered for stock_index=" + std::to_string(pos.stock_index)
			+ ": loss_ratio=" + std::to_string(loss_ratio);
	}
	return result;
}

TradeExecutor::RiskCheckResult TradeExecutor::CheckTakeProfit(const PositionRecord& pos, double current_price) const
{
	RiskCheckResult result;
	if (pos.avg_cost <= 0.0) return result;

	double profit_ratio = (current_price - pos.avg_cost) / pos.avg_cost;
	if (profit_ratio > Configer::GetStrategyConfiger().GetSingleStockTakeProfit()) {
		result.passed = false;
		result.reason = "Take profit triggered for stock_index=" + std::to_string(pos.stock_index)
			+ ": profit_ratio=" + std::to_string(profit_ratio);
	}
	return result;
}

int TradeExecutor::ExecuteBuy(int stock_index, double price, int target_shares, int trade_date)
{
	if (target_shares <= 0 || price <= 0.0) {
		LOG_WARN("TradeExecutor::ExecuteBuy - invalid parameters: shares={}, price={}", target_shares, price);
		return 0;
	}

	// 计算交易成本
	TransactionCostResult cost;
	double total_required = 0.0;
	auto calc_cost_and_required = [&]() {
		cost = CalcTransactionCost(price, target_shares, true);
		total_required = price * target_shares + cost.total;
	};
	calc_cost_and_required();

	// 检查可用资金
	if (total_required > data_manager.available_capital) {
		double max_notional = data_manager.available_capital * 0.99;
		if (max_notional <= 0.0) {
			LOG_WARN("TradeExecutor::ExecuteBuy - insufficient capital for stock_index={}", stock_index);
			return 0;
		}
		int adjusted_shares = static_cast<int>(max_notional / price);
		adjusted_shares = (adjusted_shares / 100) * 100;  // A 股一手 = 100 股
		if (adjusted_shares <= 0) {
			LOG_WARN("TradeExecutor::ExecuteBuy - insufficient capital after adjustment for stock_index={}", stock_index);
			return 0;
		}
		target_shares = adjusted_shares;
		calc_cost_and_required();

		// 调整后仍超支（极端费率场景）：放弃本次买入
		if (total_required > data_manager.available_capital) {
			LOG_WARN("TradeExecutor::ExecuteBuy - still insufficient after adjustment for stock_index={}", stock_index);
			return 0;
		}
	}

	RecordTrade(trade_date, stock_index, true, target_shares, price, cost);

	// 更新账户状态
	data_manager.available_capital -= total_required;

	// 更新持仓
	auto it = std::find_if(data_manager.positions.begin(), data_manager.positions.end(),
		[stock_index](const PositionRecord& p) { return p.stock_index == stock_index; });

	if (it != data_manager.positions.end()) {
		// 已有持仓，增加数量，更新平均成本
		double old_notional = it->avg_cost * it->shares;
		double new_notional = price * target_shares;
		it->shares += target_shares;
		it->avg_cost = (old_notional + new_notional + cost.total) / it->shares;
		it->current_value = it->shares * price;
	}
	else {
		// 新开仓
		PositionRecord new_pos;
		new_pos.stock_index = stock_index;
		new_pos.shares = target_shares;
		new_pos.avg_cost = (price * target_shares + cost.total) / target_shares;
		new_pos.current_value = price * target_shares;
		new_pos.pnl = 0.0;
		data_manager.positions.push_back(new_pos);
	}

	RecalcNetValue();

	LOG_DEBUG("BUY  | date={} | stock_idx={} | shares={} | price={:.2f} | cost={:.2f} | cash={:.2f}",
		trade_date, stock_index, target_shares, price, cost.total, data_manager.available_capital);

	return target_shares;
}

int TradeExecutor::ExecuteSell(int stock_index, double price, int target_shares, int trade_date)
{
	if (target_shares <= 0 || price <= 0.0) {
		return 0;
	}

	// 查找持仓
	auto it = std::find_if(data_manager.positions.begin(), data_manager.positions.end(),
		[stock_index](const PositionRecord& p) { return p.stock_index == stock_index; });

	if (it == data_manager.positions.end()) {
		LOG_WARN("TradeExecutor::ExecuteSell - position not found for stock_index={}", stock_index);
		return 0;
	}

	// 限制卖出股数不超过持仓
	int actual_shares = std::min(target_shares, it->shares);
	if (actual_shares <= 0) return 0;

	// 计算交易成本
	TransactionCostResult cost = CalcTransactionCost(price, actual_shares, false);
	double proceeds = price * actual_shares - cost.total;

	RecordTrade(trade_date, stock_index, false, actual_shares, price, cost);

	// 更新账户状态
	data_manager.available_capital += proceeds;

	// 更新持仓盈亏（已实现盈亏）
	double realized_pnl = (price - it->avg_cost) * actual_shares - cost.total;
	it->pnl += realized_pnl;
	it->shares -= actual_shares;
	it->current_value = it->shares * price;

	// 如果清仓，移除记录
	if (it->shares <= 0) {
		data_manager.positions.erase(it);
	}

	RecalcNetValue();

	LOG_DEBUG("SELL | date={} | stock_idx={} | shares={} | price={:.2f} | cost={:.2f} | proceeds={:.2f} | cash={:.2f}",
		trade_date, stock_index, actual_shares, price, cost.total, proceeds, data_manager.available_capital);

	return actual_shares;
}

void TradeExecutor::ClosePosition(int stock_index, double price, int trade_date)
{
	ExecuteSell(stock_index, price, INT_MAX, trade_date);
}

void TradeExecutor::CloseAllPositions(int trade_date, const std::vector<double>& prices)
{
	// 复制一份持仓列表，避免在遍历时修改
	auto positions_copy = data_manager.positions;
	for (const auto& pos : positions_copy) {
		if (pos.stock_index >= 0 && pos.stock_index < static_cast<int>(prices.size())) {
			ClosePosition(pos.stock_index, prices[pos.stock_index], trade_date);
		}
	}
}

void TradeExecutor::UpdateMarketValue(int stock_index, double close_price)
{
	for (auto& pos : data_manager.positions) {
		if (pos.stock_index == stock_index) {
			pos.current_value = pos.shares * close_price;
			break;
		}
	}
}

void TradeExecutor::UpdateAllMarketValues(const std::vector<double>& close_prices)
{
	for (auto& pos : data_manager.positions) {
		if (pos.stock_index >= 0 && pos.stock_index < static_cast<int>(close_prices.size())) {
			pos.current_value = pos.shares * close_prices[pos.stock_index];
		}
	}
	RecalcNetValue();
}

void TradeExecutor::RecordTrade(int trade_date, int stock_index, bool is_buy,
	int shares, double price, const TransactionCostResult& cost)
{
	TradeRecord trade;
	trade.trade_date = trade_date;
	trade.stock_index = stock_index;
	trade.is_buy = is_buy;
	trade.shares = shares;
	trade.price = price;
	trade.commission = cost.commission;
	trade.stamp_duty = cost.stamp_duty;
	trade.transfer_fee = cost.transfer_fee;
	trade.slippage = cost.slippage;
	trade.total_cost = cost.total;
	data_manager.trade_history.push_back(trade);
}

void TradeExecutor::RecalcNetValue()
{
	data_manager.total_net_value = data_manager.available_capital;
	for (const auto& pos : data_manager.positions) {
		data_manager.total_net_value += pos.current_value;
	}
}

void TradeExecutor::RecordDailySnapshot(int trade_date)
{
	NetValueSnapshot snapshot;
	snapshot.trade_date = trade_date;
	snapshot.total_net_value = data_manager.total_net_value;
	snapshot.market_value = data_manager.total_net_value - data_manager.available_capital;
	snapshot.cash = data_manager.available_capital;

	// 计算日收益率和累计收益率
	if (!data_manager.nav_history.empty()) {
		double prev_nav = data_manager.nav_history.back().total_net_value;
		if (prev_nav > 0.0) {
			snapshot.daily_return = (data_manager.total_net_value - prev_nav) / prev_nav;
		}
	}
	if (data_manager.initial_capital > 0.0) {
		snapshot.cumulative_return = (data_manager.total_net_value - data_manager.initial_capital) / data_manager.initial_capital;
	}

	data_manager.nav_history.push_back(snapshot);
}

void TradeExecutor::ProcessCorporateActions(PositionRecord& pos, int date_abs_idx)
{
	int stock_index = pos.stock_index;
	GlobalData* gd = GlobalData::GetGlobalData();

	StockKData* skd = gd->get_stock_k_data(stock_index);
	if (!skd) return;

	const auto& fin_datas = skd->get_financial_datas();

	if (date_abs_idx < 0 || date_abs_idx >= static_cast<int>(fin_datas.size())) {
		LOG_WARN("ProcessCorporateActions - date_abs_idx={} out of range for financial_datas (size={}), stock_idx={}",
			date_abs_idx, fin_datas.size(), stock_index);
		return;
	}
	const auto& fin = fin_datas[date_abs_idx];

	// === 1. 拆股/送股处理 ===
	if (std::abs(fin.split_ratio - 1.0) > 1e-10) {
		int old_shares = pos.shares;
		pos.shares = static_cast<int>(pos.shares * fin.split_ratio);
		pos.avg_cost /= fin.split_ratio; // 成本按比例下调

		LOG_DEBUG("CORP_ACT SPLIT | date_abs={} | stock_idx={} | split_ratio={:.4f} | shares {} -> {} | avg_cost {:.4f} -> {:.4f}",
			date_abs_idx, stock_index, fin.split_ratio,
			old_shares, pos.shares,
			old_shares > 0 ? pos.avg_cost * fin.split_ratio : 0.0, pos.avg_cost);
	}

	// === 2. 现金分红处理 ===
	if (fin.cash_dividend > 1e-10) {
		double dividend = fin.cash_dividend * pos.shares;

		data_manager.available_capital += dividend;
		data_manager.total_net_value += dividend;

		// 分红计入已实现盈亏
		pos.pnl += dividend;

		LOG_DEBUG("CORP_ACT DIVIDEND | date_abs={} | stock_idx={} | dividend_per_share={:.4f} | shares={} | total_dividend={:.2f} | cash={:.2f}",
			date_abs_idx, stock_index, fin.cash_dividend, pos.shares, dividend, data_manager.available_capital);
	}
}

void TradeExecutor::ProcessAllCorporateActions(int date_abs_idx)
{
	for (auto& pos : data_manager.positions) {
		ProcessCorporateActions(pos, date_abs_idx);
	}
}

void TradeExecutor::CheckAllStopLossTakeProfit(const std::vector<double>& current_prices, int trade_date)
{
	// 复制一份持仓列表，遍历时可能修改原列表
	auto positions_copy = data_manager.positions;
	for (const auto& pos : positions_copy) {
		if (pos.stock_index < 0 || pos.stock_index >= static_cast<int>(current_prices.size())) continue;
		double price = current_prices[pos.stock_index];
		if (price <= 0.0) continue;

		// 止损检查
		RiskCheckResult sl_result = CheckStopLoss(pos, price);
		if (!sl_result.passed) {
			LOG_DEBUG("STOP LOSS triggered: {}", sl_result.reason);
			ClosePosition(pos.stock_index, price, trade_date);
			continue;
		}

		// 止盈检查
		RiskCheckResult tp_result = CheckTakeProfit(pos, price);
		if (!tp_result.passed) {
			LOG_DEBUG("TAKE PROFIT triggered: {}", tp_result.reason);
			ClosePosition(pos.stock_index, price, trade_date);
		}
	}
}

void TradeExecutor::Rebalance(const std::vector<int>& target_indices,
	const std::vector<double>& prices,
	int trade_date)
{
	if (prices.empty()) {
		LOG_ERROR("TradeExecutor::Rebalance - prices vector is empty");
		return;
	}

	// 记录调仓前净值，用于计算换手率
	double pre_nav = data_manager.total_net_value;
	size_t trade_count_before = data_manager.trade_history.size();

	// 第一步：卖出不在目标列表中的股票
	auto positions_copy = data_manager.positions;
	for (const auto& pos : positions_copy) {
		bool is_target = std::find(target_indices.begin(), target_indices.end(), pos.stock_index) != target_indices.end();
		if (!is_target) {
			if (pos.stock_index >= 0 && pos.stock_index < static_cast<int>(prices.size())) {
				ClosePosition(pos.stock_index, prices[pos.stock_index], trade_date);
			}
		}
	}

	// 第二步：买入目标列表中的股票（等权分配资金）
	if (target_indices.empty()) return;

	// 计算调仓总可用资金
	double rebalance_capital = data_manager.available_capital;
	// 减去已持有的目标股票的市值（保持现有持仓）
	for (int idx : target_indices) {
		auto* pos = data_manager.GetPosition(idx);
		if (pos) {
			rebalance_capital += pos->current_value;
		}
	}

	// 等权分配到每只目标股票
	double target_notional_per_stock = rebalance_capital / target_indices.size();

	const auto& strategy_cfg = Configer::GetStrategyConfiger();
	GlobalData* gd = GlobalData::GetGlobalData();

	for (int idx : target_indices) {
		if (idx < 0 || idx >= static_cast<int>(prices.size())) continue;
		double price = prices[idx];
		if (price <= 0.0) continue;

		// 每次迭代重算，反映此前 trim 卖出导致的净值变化
		double single_limit_nv = data_manager.total_net_value * strategy_cfg.GetSinglePositionLimit();
		double industry_limit_nv = data_manager.total_net_value * strategy_cfg.GetIndustryPositionLimit();

		// 检查是否已持有该股票
		auto* existing_pos = data_manager.GetPosition(idx);
		double existing_value = existing_pos ? existing_pos->current_value : 0.0;
		double remaining_budget = target_notional_per_stock - existing_value;

		// 最大允许持仓 = min(等权目标, 单票仓位上限)
		double max_allowed = std::min(target_notional_per_stock, single_limit_nv);

		if (existing_value > max_allowed) {
			// 超目标或超上限：卖出超出部分
			double excess_notional = existing_value - max_allowed;
			int trim_shares = static_cast<int>(excess_notional / price);
			trim_shares = (trim_shares / 100) * 100;
			if (trim_shares > 0) {
				ExecuteSell(idx, price, trim_shares, trade_date);
			}
			continue;
		}
		if (remaining_budget <= 0.0) continue;

		// 单票仓位上限检查（此时 existing_value <= max_allowed，受上限约束后允许的买入额）
		double allowed_notional = std::min(remaining_budget, single_limit_nv - existing_value);
		if (allowed_notional <= 0.0) continue;

		// 行业仓位上限检查
		if (gd) {
			StockKData* skd = gd->get_stock_k_data(idx);
			if (skd) {
				int32_t ind_code = skd->GetIndustryCode();
				if (ind_code != 0) {
					// 统计该行业在所有持仓中的已有市值
					double industry_existing = 0.0;
					for (const auto& pos : data_manager.positions) {
						if (pos.shares <= 0) continue;
						StockKData* pos_skd = gd->get_stock_k_data(pos.stock_index);
						if (pos_skd && pos_skd->GetIndustryCode() == ind_code) {
							industry_existing += pos.current_value;
						}
					}
					// 当前股票如果已持有，不减自己的市值（上面 allowed_notional 已包含）
					double industry_budget = industry_limit_nv - industry_existing;
					if (industry_budget <= 0.0) {
						LOG_DEBUG("TradeExecutor::Rebalance - industry limit reached for stock_idx={} industry={}", idx, ind_code);
						continue;
					}
					allowed_notional = std::min(allowed_notional, industry_budget);
				}
			}
		}

		if (allowed_notional <= 0.0) continue;

		int target_shares = static_cast<int>(allowed_notional / price);
		target_shares = (target_shares / 100) * 100;  // A 股一手 = 100 股
		if (target_shares <= 0) continue;

		ExecuteBuy(idx, price, target_shares, trade_date);
	}

	// 计算本次调仓换手率
	double total_traded = 0.0;
	for (size_t t = trade_count_before; t < data_manager.trade_history.size(); ++t) {
		total_traded += data_manager.trade_history[t].price * data_manager.trade_history[t].shares;
	}
	if (pre_nav > 0.0) {
		data_manager.turnover_rates.push_back(total_traded / (2.0 * pre_nav));
	}

	LOG_DEBUG("REBALANCE | date={} | target_count={} | cash_after={:.2f} | nav={:.2f}",
		trade_date, target_indices.size(), data_manager.available_capital, data_manager.total_net_value);
}

void TradeExecutor::SetInitialCapital(double capital)
{
	data_manager.initial_capital = capital;
	data_manager.available_capital = capital;
	data_manager.total_net_value = capital;
}

void TradeExecutor::ReInitialize(double init_capital)
{
	data_manager.positions.clear();
	data_manager.trade_history.clear();
	data_manager.nav_history.clear();
	data_manager.turnover_rates.clear();
	SetInitialCapital(init_capital);

	LOG_DEBUG("TradeExecutor::ReInitialize - reset to init_capital={:.2f}", init_capital);
}
