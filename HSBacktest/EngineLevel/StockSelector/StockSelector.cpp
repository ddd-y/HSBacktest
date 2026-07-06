#include "StockSelector.h"
#include "../../Datalevel/Global_data.h"
#include "../../Datalevel/stock_k_data.h"
#include "../../Datalevel/data_defs.h"
#include "../../ConfigLvevl/configer.h"
#include "../../MyLog/Logger.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_map>

bool StockSelector::IsTradable(const StockKData& kdata, int data_idx) const
{
	const auto& daily = kdata.get_daily_datas();
	if (data_idx < 0 || data_idx >= static_cast<int>(daily.size())) return false;

	const StockDailyData& d = daily[data_idx];

	if (d.is_delisted != 0) return false;
	if (d.is_suspended != 0) return false;
	if (d.is_limit_up != 0) return false;

	return true;
}

std::vector<StockScoreRecord> StockSelector::ScoreAllStocks(int rebalance_idx) const
{
	GlobalData* gd = GlobalData::GetGlobalData();
	if (!gd) {
		LOG_ERROR("StockSelector::ScoreAllStocks - GlobalData not initialized");
		return {};
	}

	int stock_count = gd->get_stock_count();
	if (stock_count <= 0) {
		LOG_WARN("StockSelector::ScoreAllStocks - no stocks in GlobalData");
		return {};
	}

	const auto& rebalance_index = gd->get_rebalance_index();
	if (rebalance_idx < 0 || rebalance_idx >= static_cast<int>(rebalance_index.size())) {
		LOG_WARN("StockSelector::ScoreAllStocks - invalid rebalance_idx");
		return {};
	}
	int data_idx = rebalance_index[rebalance_idx];

	// 第一步：构建可交易掩码
	std::vector<bool> tradable_mask(stock_count, false);
	int tradable_count = 0;
	for (int i = 0; i < stock_count; ++i) {
		const StockKData* kdata = gd->get_stock_k_data(i);
		if (kdata && IsTradable(*kdata, data_idx)) {
			tradable_mask[i] = true;
			++tradable_count;
		}
	}
	LOG_DEBUG("StockSelector::ScoreAllStocks - {} / {} stocks are tradable at rebalance_idx={}",
		tradable_count, stock_count, rebalance_idx);

	if (tradable_count == 0) return {};

	// 第二步：获取权重
	std::array<double, FACTOR_NUM> weights = gd->GetWeights(adjustParamIndex);
	if (Configer::GetStrategyConfiger().GetAutoNormalizeWeights()) {
		double weight_sum = std::accumulate(weights.begin(), weights.end(), 0.0);
		if (weight_sum > 0.0) {
			for (auto& w : weights) w /= weight_sum;
		}
	}

	// ===== Pass 1: 一趟同时累加所有因子的 sum 和平方和 =====
	std::array<double, FACTOR_NUM> sum{};
	std::array<double, FACTOR_NUM> sq_sum{};
	int count = 0;

	for (int i = 0; i < stock_count; ++i) {
		if (!tradable_mask[i]) continue;
		auto raw = gd->GetValue(i, rebalance_idx);
		for (int f = 0; f < FACTOR_NUM; ++f) {
			double v = raw[f];
			sum[f] += v;
			sq_sum[f] += v * v;
		}
		++count;
	}

	// 计算各因子均值与标准差（Var(X) = E[X²] - E[X]²）
	std::array<double, FACTOR_NUM> mean{}, stddev{};
	for (int f = 0; f < FACTOR_NUM; ++f) {
		mean[f] = sum[f] / count;
		double variance = sq_sum[f] / count - mean[f] * mean[f];
		if (variance < 1e-10) variance = 1e-10;
		stddev[f] = std::sqrt(variance);
	}

	// ===== Pass 2: z-score + 3-sigma 截断 + 合成复合得分，直接产出结果 =====
	std::vector<StockScoreRecord> results;
	results.reserve(tradable_count);

	for (int i = 0; i < stock_count; ++i) {
		if (!tradable_mask[i]) continue;

		auto raw = gd->GetValue(i, rebalance_idx);
		StockScoreRecord rec;
		rec.stock_index = i;
		rec.composite_score = 0.0;

		for (int f = 0; f < FACTOR_NUM; ++f) {
			double z = (raw[f] - mean[f]) / stddev[f];
			if (z > 3.0) z = 3.0;
			if (z < -3.0) z = -3.0;
			rec.factor_scores[f] = z;
			rec.composite_score += z * weights[f];
		}
		results.push_back(std::move(rec));
	}

	return results;
}

std::vector<int> StockSelector::SelectTopN(const std::vector<StockScoreRecord>& scores, int top_n) const
{
	if (scores.empty() || top_n <= 0) return {};

	int n = std::min(top_n, static_cast<int>(scores.size()));

	// partial_sort: O(N log n) vs 原来 std::sort 的 O(N log N)
	std::vector<StockScoreRecord> sorted = scores;
	auto mid = sorted.begin() + n;
	std::partial_sort(sorted.begin(), mid, sorted.end(),
		[](const StockScoreRecord& a, const StockScoreRecord& b) {
			return a.composite_score > b.composite_score;
		});

	std::vector<int> selected(n);
	for (int i = 0; i < n; ++i) {
		selected[i] = sorted[i].stock_index;
	}

	LOG_DEBUG("StockSelector::SelectTopN - selected {} stocks from {} candidates", n, scores.size());
	return selected;
}

std::vector<int> StockSelector::SelectIndustryNeutral(
	const std::vector<StockScoreRecord>& scores,
	int total_target,
	int min_per_industry) const
{
	if (scores.empty() || total_target <= 0) return {};

	GlobalData* gd = GlobalData::GetGlobalData();
	if (!gd) return SelectTopN(scores, total_target);  // fallback

	// 1. 按行业分组（unordered_map O(1) 插入，避免 std::map 的 rb-tree 开销）
	std::unordered_map<int32_t, std::vector<StockScoreRecord>> industry_groups;
	for (const auto& rec : scores) {
		StockKData* skd = gd->get_stock_k_data(rec.stock_index);
		int32_t ind_code = skd ? skd->GetIndustryCode() : 0;
		if (ind_code == 0) ind_code = -1;  // 未分类归为一组
		industry_groups[ind_code].push_back(rec);
	}

	int num_industries = static_cast<int>(industry_groups.size());
	if (num_industries <= 1) {
		LOG_DEBUG("StockSelector::SelectIndustryNeutral - only {} industry group(s), fallback to global ranking", num_industries);
		return SelectTopN(scores, total_target);
	}

	int effective_min = min_per_industry;
	if (min_per_industry * num_industries > total_target) {
		effective_min = total_target / num_industries;
		LOG_WARN("StockSelector::SelectIndustryNeutral - min_per_industry({}) x industries({}) = {} > top_n({}), "
		         "capped to {}", min_per_industry, num_industries,
		         min_per_industry * num_industries, total_target, effective_min);
	}

	// 2. 每个行业内部排序
	for (auto& [ind, stocks] : industry_groups) {
		std::sort(stocks.begin(), stocks.end(),
			[](const StockScoreRecord& a, const StockScoreRecord& b) {
				return a.composite_score > b.composite_score;
			});
	}

	// 3. vector<bool> 代替 std::set：O(1) 插入/查找，零析构开销
	int stock_count = gd->get_stock_count();
	std::vector<bool> selected_mask(stock_count, false);
	int selected_count = 0;
	std::unordered_map<int32_t, int> industry_pick_count;

	for (auto& [ind, stocks] : industry_groups) {
		int take = std::min(effective_min, static_cast<int>(stocks.size()));
		for (int i = 0; i < take; ++i) {
			selected_mask[stocks[i].stock_index] = true;
			++selected_count;
		}
		industry_pick_count[ind] = take;
	}

	// 4. 第二轮：剩余名额按全局得分补足（存指针避免拷贝 StockScoreRecord）
	int remaining = total_target - selected_count;
	if (remaining > 0) {
		std::vector<const StockScoreRecord*> remaining_candidates;
		remaining_candidates.reserve(scores.size() - selected_count);
		for (const auto& rec : scores) {
			if (!selected_mask[rec.stock_index]) {
				remaining_candidates.push_back(&rec);
			}
		}
		std::sort(remaining_candidates.begin(), remaining_candidates.end(),
			[](const StockScoreRecord* a, const StockScoreRecord* b) {
				return a->composite_score > b->composite_score;
			});

		int take = std::min(remaining, static_cast<int>(remaining_candidates.size()));
		for (int i = 0; i < take; ++i) {
			selected_mask[remaining_candidates[i]->stock_index] = true;
		}
	}

	// 从 mask 收集结果（保持 scores 的相对顺序）
	std::vector<int> result;
	result.reserve(total_target);
	for (const auto& rec : scores) {
		if (selected_mask[rec.stock_index]) {
			result.push_back(rec.stock_index);
		}
	}

	// 日志
	std::string industry_log;
	for (auto& [ind, count] : industry_pick_count) {
		industry_log += "ind=" + std::to_string(ind) + ":" + std::to_string(count) + " ";
	}
	LOG_DEBUG("StockSelector::SelectIndustryNeutral - {} industries, selected {} stocks (target={}, min_per_ind={}) | {}",
		num_industries, result.size(), total_target, effective_min, industry_log);

	return result;
}

std::vector<int> StockSelector::ScoreAndSelect(int rebalance_idx, int* actual_selected) const
{
	GlobalData* gd = GlobalData::GetGlobalData();
	int top_n = gd->GetTopN(adjustParamIndex);

	std::vector<StockScoreRecord> scores = ScoreAllStocks(rebalance_idx);

	if (scores.empty()) {
		LOG_WARN("StockSelector::ScoreAndSelect - no tradable stocks at rebalance_idx={}", rebalance_idx);
		if (actual_selected) *actual_selected = 0;
		return {};
	}

	int effective_top_n = std::min(top_n, static_cast<int>(scores.size()));
	if (actual_selected) *actual_selected = effective_top_n;

	if (effective_top_n < top_n) {
		LOG_DEBUG("StockSelector::ScoreAndSelect - tradable ({}) < top_n ({}), capped to {}",
			static_cast<int>(scores.size()), top_n, effective_top_n);
	}

	return SelectIndustryNeutral(scores, effective_top_n,
		Configer::GetStrategyConfiger().GetMinStocksPerIndustry());
}
