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

std::vector<int> StockSelector::ScoreAndSelect(int rebalance_idx, int* actual_selected) const
{
	GlobalData* gd = GlobalData::GetGlobalData();
	int top_n = gd ? gd->GetTopN(adjustParamIndex) : 50;

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

	return SelectTopN(scores, effective_top_n);
}
