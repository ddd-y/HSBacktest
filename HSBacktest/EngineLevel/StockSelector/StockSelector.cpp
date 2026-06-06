#include "StockSelector.h"
#include "../../Datalevel/Global_data.h"
#include "../../Datalevel/stock_k_data.h"
#include "../../Datalevel/data_defs.h"
#include "../../ConfigLvevl/configer.h"
#include "../../MyLog/Logger.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <map>
#include <set>

void StockSelector::CrossSectionalNormalize(std::vector<double>& values, const std::vector<bool>* tradable_mask) const
{
	if (values.empty()) return;

	// 仅对可交易的股票计算均值和标准差
	double sum = 0.0;
	int count = 0;
	for (size_t i = 0; i < values.size(); ++i) {
		if (tradable_mask && !(*tradable_mask)[i]) continue;
		sum += values[i];
		++count;
	}
	if (count == 0) return;

	double mean = sum / count;

	double sq_sum = 0.0;
	for (size_t i = 0; i < values.size(); ++i) {
		if (tradable_mask && !(*tradable_mask)[i]) continue;
		double diff = values[i] - mean;
		sq_sum += diff * diff;
	}
	double stddev = std::sqrt(sq_sum / count);

	if (stddev < 1e-10) {
		for (size_t i = 0; i < values.size(); ++i) {
			if (!tradable_mask || (*tradable_mask)[i])
				values[i] = 0.0;
		}
		return;
	}

	// 3-sigma截断 + Z-score标准化（仅对可交易股票）
	for (size_t i = 0; i < values.size(); ++i) {
		if (tradable_mask && !(*tradable_mask)[i]) {
			values[i] = std::numeric_limits<double>::lowest();
			continue;
		}
		double z = (values[i] - mean) / stddev;
		if (z > 3.0) z = 3.0;
		if (z < -3.0) z = -3.0;
		values[i] = z;
	}
}

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
	LOG_INFO("StockSelector::ScoreAllStocks - {} / {} stocks are tradable at rebalance_idx={}",
		tradable_count, stock_count, rebalance_idx);

	// 第二步：获取权重（直接从 GlobalData 拿）
	std::array<double, FACTOR_NUM> weights = gd->GetWeights(adjustParamIndex);

	// 权重归一化
	if (Configer::GetStrategyConfiger().GetAutoNormalizeWeights()) {
		double weight_sum = std::accumulate(weights.begin(), weights.end(), 0.0);
		if (weight_sum > 0.0) {
			for (auto& w : weights) w /= weight_sum;
		}
	}

	// 第三步：收集所有股票的因子值（直接从 GlobalData::GetValue 拿）
	std::array<std::vector<double>,FACTOR_NUM> factor_values_by_type;
	for (int f = 0; f < FACTOR_NUM; ++f) {
		factor_values_by_type[f].resize(stock_count, 0.0);
	}

	for (int i = 0; i < stock_count; ++i) {
		std::array<double, FACTOR_NUM> raw_values = gd->GetValue(i, rebalance_idx);
		for (int f = 0; f < FACTOR_NUM; ++f) {
			factor_values_by_type[f][i] = raw_values[f];
		}
	}

	// 第四步：横截面标准化（所有因子都参与）
	for (int f = 0; f < FACTOR_NUM; ++f) {
		CrossSectionalNormalize(factor_values_by_type[f], &tradable_mask);
	}

	// 第五步：合成复合得分（仅保留可交易股票）
	std::vector<StockScoreRecord> results;
	results.reserve(tradable_count);

	for (int i = 0; i < stock_count; ++i) {
		if (!tradable_mask[i]) continue;

		StockScoreRecord rec;
		rec.stock_index = i;
		rec.composite_score = 0.0;

		for (int f = 0; f < FACTOR_NUM; ++f) {
			rec.factor_scores[f] = factor_values_by_type[f][i];
			rec.composite_score += factor_values_by_type[f][i] * weights[f];
		}
		results.push_back(std::move(rec));
	}

	return results;
}

std::vector<int> StockSelector::SelectTopN(const std::vector<StockScoreRecord>& scores, int top_n) const
{
	if (scores.empty() || top_n <= 0) return {};

	std::vector<StockScoreRecord> sorted = scores;
	std::sort(sorted.begin(), sorted.end(),
		[](const StockScoreRecord& a, const StockScoreRecord& b) {
			return a.composite_score > b.composite_score;
		});

	int n = std::min(top_n, static_cast<int>(sorted.size()));
	std::vector<int> selected(n);
	for (int i = 0; i < n; ++i) {
		selected[i] = sorted[i].stock_index;
	}

	LOG_INFO("StockSelector::SelectTopN - selected {} stocks from {} candidates", n, scores.size());
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

	// 1. 按行业分组，每组内按得分降序排列
	std::map<int32_t, std::vector<StockScoreRecord>> industry_groups;
	for (const auto& rec : scores) {
		StockKData* skd = gd->get_stock_k_data(rec.stock_index);
		int32_t ind_code = skd ? skd->GetIndustryCode() : 0;
		if (ind_code == 0) ind_code = -1;  // 未分类归为一组
		industry_groups[ind_code].push_back(rec);
	}

	int num_industries = static_cast<int>(industry_groups.size());
	if (num_industries <= 1) {
		// 只有一个行业或没有行业数据，回退到全局排名
		LOG_INFO("StockSelector::SelectIndustryNeutral - only {} industry group(s), fallback to global ranking", num_industries);
		return SelectTopN(scores, total_target);
	}

	// 校验：min_per_industry × 行业数 不能超过 total_target
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

	// 3. 第一轮：每个行业先取 effective_min 只
	std::set<int> selected_set;        // 去重
	std::map<int32_t, int> industry_pick_count;  // 每个行业已选数量

	for (auto& [ind, stocks] : industry_groups) {
		int take = std::min(effective_min, static_cast<int>(stocks.size()));
		for (int i = 0; i < take; ++i) {
			selected_set.insert(stocks[i].stock_index);
		}
		industry_pick_count[ind] = take;
	}

	// 4. 第二轮：剩余名额按全局得分补足
	int remaining = total_target - static_cast<int>(selected_set.size());
	if (remaining > 0) {
		// 所有已选的跳过，未选的按全局得分排序
		std::vector<StockScoreRecord> remaining_candidates;
		for (const auto& rec : scores) {
			if (selected_set.count(rec.stock_index) == 0) {
				remaining_candidates.push_back(rec);
			}
		}
		std::sort(remaining_candidates.begin(), remaining_candidates.end(),
			[](const StockScoreRecord& a, const StockScoreRecord& b) {
				return a.composite_score > b.composite_score;
			});

		int take = std::min(remaining, static_cast<int>(remaining_candidates.size()));
		for (int i = 0; i < take; ++i) {
			selected_set.insert(remaining_candidates[i].stock_index);
		}
	}

	std::vector<int> result(selected_set.begin(), selected_set.end());

	// 日志：输出每个行业的选股数
	std::string industry_log;
	for (auto& [ind, count] : industry_pick_count) {
		industry_log += "ind=" + std::to_string(ind) + ":" + std::to_string(count) + " ";
	}
	LOG_INFO("StockSelector::SelectIndustryNeutral - {} industries, selected {} stocks (target={}, min_per_ind={}) | {}",
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
		LOG_WARN("StockSelector::ScoreAndSelect - tradable ({}) < top_n ({}), capped to {}",
			static_cast<int>(scores.size()), top_n, effective_top_n);
	}

	return SelectIndustryNeutral(scores, effective_top_n,
		Configer::GetStrategyConfiger().GetMinStocksPerIndustry());
}
