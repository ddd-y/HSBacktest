#pragma once
#include <array>
#include <vector>
#include <string>
#include <memory>
#include "../../Datalevel/factor_calculate/factorbase.h"

class StockKData;

// ==========================================
// 股票评分记录
// ==========================================
struct StockScoreRecord {
	int stock_index = -1;       // 在GlobalData中的索引
	double composite_score = 0.0; // 复合因子总分
	std::array<double, FACTOR_NUM> factor_scores{}; // 各因子标准化得分（用于调试和分析）
};

// ==========================================
// StockSelector：选股器
// 职责：
// 1. 在调仓日从GlobalData读取各股票的因子值
// 2. 执行横截面标准化（Z-score）
// 3. 按权重合成复合因子得分
// 4. 排序并选出Top N股票
// ==========================================
class StockSelector {
public:
	StockSelector() = default;

	StockSelector(int n_adjustParamIndex): adjustParamIndex(n_adjustParamIndex)
	{}
	// ===== 主要接口 =====

	// 在指定调仓日对所有可交易股票进行评分（因子值和权重直接从GlobalData单例获取）
	// 不可交易（退市/停牌/涨停）的股票不会出现在结果中
	// @param rebalance_idx 当前调仓日在rebalance_index中的位置（第几个调仓日）
	// @return 仅包含可交易股票的评分结果
	std::vector<StockScoreRecord> ScoreAllStocks(int rebalance_idx) const;

	// 选择Top N股票（最多返回 top_n 个，若可选股票不足则返回全部）
	std::vector<int> SelectTopN(const std::vector<StockScoreRecord>& scores, int top_n) const;

	// 一站式接口：评分+选股一步完成
	// top_n 和 weights 都通过 adjustParamIndex 从 GlobalData 获取
	// @param rebalance_idx 当前调仓日索引
	// @param actual_selected [out] 返回实际选中的股票数量（可 < top_n）
	// @return 选中股票的stock_index列表
	std::vector<int> ScoreAndSelect(int rebalance_idx, int* actual_selected = nullptr) const;

	// 重置参数索引（同时切换 weights 和 top_n）
	// @param n_adjustParamIndex 调整参数索引
	void ReSetAdjustParamIndex(int n_adjustParamIndex)
	{
		adjustParamIndex = n_adjustParamIndex;
	}

	// 获取当前参数索引
	int GetAdjustParamIndex() const { return adjustParamIndex; }

private:
	// ===== 内部方法 =====

	// 横截面Z-score标准化（去除异常值影响，使用MAD或3-sigma截断）
	// @param values 所有股票在某因子上的值（按stock_index排列）
	// @param tradable_mask 可交易标记，tradable_mask[i]=true 表示该股票可交易；为空则全部纳入标准化
	void CrossSectionalNormalize(std::vector<double>& values, const std::vector<bool>* tradable_mask = nullptr) const;

	// 判断指定股票在指定调仓日是否可交易（非退市、非停牌、非涨停）
	bool IsTradable(const StockKData& kdata, int data_idx) const;

	// 调整参数索引，同时用于 GetWeights 和 GetTopN
	int adjustParamIndex = 0;
};
