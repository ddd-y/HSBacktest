#pragma once
#include<string>
#include<vector>

// ==========================================
// 参数搜索配置（用于并行参数优化）
// ==========================================
class ParamSearchConfiger
{
public:
    enum class SearchMode { GRID, RANDOM, SINGLE_FACTOR };

private:
    SearchMode mode = SearchMode::RANDOM;

    //各因子权重范围
    double momentum_weight_min  = 0.0,  momentum_weight_max  = 1.0;
    double turnover_weight_min  = 0.0,  turnover_weight_max  = 1.0;
    double volatility_weight_min = 0.0, volatility_weight_max = 1.0;
    double mcap_weight_min      = 0.0,  mcap_weight_max      = 1.0;
    double ep_weight_min        = 0.0,  ep_weight_max        = 1.0;

    //网格搜索步长
    double grid_step = 0.1;

    //随机采样数量
    int random_samples = 1000;

    //是否归一化（权重之和=1）
    bool normalize_weights = true;

    //是否允许权重为0
    bool allow_zero_weight = true;

    //top_n候选值列表
    std::vector<int> top_n_candidates = { 20, 30, 50, 80, 100 };

    //随机模式下top_n是否也随机选取（为false则为每个权重组合生成所有top_n候选）
    bool random_top_n = false;

public:
    // Getter / Setter
    SearchMode GetMode() const { return mode; }
    void SetMode(SearchMode m) { mode = m; }

    double GetMomentumWeightMin() const { return momentum_weight_min; }
    void SetMomentumWeightMin(double v) { momentum_weight_min = v; }
    double GetMomentumWeightMax() const { return momentum_weight_max; }
    void SetMomentumWeightMax(double v) { momentum_weight_max = v; }

    double GetTurnoverWeightMin() const { return turnover_weight_min; }
    void SetTurnoverWeightMin(double v) { turnover_weight_min = v; }
    double GetTurnoverWeightMax() const { return turnover_weight_max; }
    void SetTurnoverWeightMax(double v) { turnover_weight_max = v; }

    double GetVolatilityWeightMin() const { return volatility_weight_min; }
    void SetVolatilityWeightMin(double v) { volatility_weight_min = v; }
    double GetVolatilityWeightMax() const { return volatility_weight_max; }
    void SetVolatilityWeightMax(double v) { volatility_weight_max = v; }

    double GetMcapWeightMin() const { return mcap_weight_min; }
    void SetMcapWeightMin(double v) { mcap_weight_min = v; }
    double GetMcapWeightMax() const { return mcap_weight_max; }
    void SetMcapWeightMax(double v) { mcap_weight_max = v; }

    double GetEpWeightMin() const { return ep_weight_min; }
    void SetEpWeightMin(double v) { ep_weight_min = v; }
    double GetEpWeightMax() const { return ep_weight_max; }
    void SetEpWeightMax(double v) { ep_weight_max = v; }

    double GetGridStep() const { return grid_step; }
    void SetGridStep(double v) { grid_step = v; }

    int GetRandomSamples() const { return random_samples; }
    void SetRandomSamples(int v) { random_samples = v; }

    bool GetNormalizeWeights() const { return normalize_weights; }
    void SetNormalizeWeights(bool v) { normalize_weights = v; }

    bool GetAllowZeroWeight() const { return allow_zero_weight; }
    void SetAllowZeroWeight(bool v) { allow_zero_weight = v; }

    const std::vector<int>& GetTopNCandidates() const { return top_n_candidates; }
    void SetTopNCandidates(const std::vector<int>& v) { top_n_candidates = v; }

    bool GetRandomTopN() const { return random_top_n; }
    void SetRandomTopN(bool v) { random_top_n = v; }

    void ReadConfigFromFile(const std::string& filename)
    {
        // 实现从JSON文件读取配置的逻辑
    }
};

class DataLevelConfiger 
{
private:
	int change_duration = 20; // 调仓周期（交易日）
public:
    int GetChangeDuration() const { return change_duration; }

    void ReadConfigFromFile(const std::string& filename)
    {
        // 实现从JSON文件读取配置的逻辑
	}
};

//暂时用不到，后边再说
class StrategyConfiger
{
    // ===== 调仓参数 =====
    int hold_days = 20;                  // 调仓周期（交易日）
    //暂时不支持调仓方式可选
    // std::string RebalanceType = "FIXED_INTERVAL"; // 调仓方式：FIXED_INTERVAL(固定间隔)/MONTH_START(月初)
    int top_n = 50;                      // 选股数量
	//暂时不支持加仓权方式可选
    // std::string WeightingMethod = "EQUAL_WEIGHT"; // 持仓加权方式：EQUAL_WEIGHT(等权)/MARKET_CAP_WEIGHT(市值加权)

    // ===== 完整因子权重体系（与DataLevelConfiger的因子开关一一对应） =====
    double momentum_weight = 0.2;
    double turnover_weight = 0.1;
    double volatility_weight = 0.1;
    double mcap_weight = 0.3;
    double ep_weight = 0.3;
    // 新增：权重自动归一化开关
    bool auto_normalize_weights = true;

    // ===== 完整风控规则 =====
    double single_position_limit = 0.02;  // 单票仓位上限
    double industry_position_limit = 0.2; // 单行业仓位上限
    double total_position_limit = 1.0;    // 总仓位上限（1.0=满仓）
    double daily_max_loss = 0.05;         // 单日最大亏损止损（5%）
    double single_stock_stop_loss = 0.1;  // 单只股票止损比例（10%）
    double single_stock_take_profit = 0.3; // 单只股票止盈比例（30%）

    // ===== 收益计算参数 =====
    double risk_free_rate = 0.03;         // 无风险利率（年化3%）
public:
    // Getter方法保持不变，补充新参数的Getter
    int GetHoldDays() const { return hold_days; }
    //std::string GetRebalanceType() { return RebalanceType; }
    int GetTopN() const { return top_n; }
    //std::string GetWeightingMethod() { return WeightingMethod; }
    // 因子权重Getter\r
    double GetMomentumWeight() const { return momentum_weight; }
    double GetTurnoverWeight() const { return turnover_weight; }
    double GetVolatilityWeight() const { return volatility_weight; }
    double GetMcapWeight() const { return mcap_weight; }
    double GetEpWeight() const { return ep_weight; }
    bool GetAutoNormalizeWeights() const { return auto_normalize_weights; }
     // 风控规则的Getter
    double GetSinglePositionLimit() const { return single_position_limit; }
    double GetIndustryPositionLimit() const { return industry_position_limit; }
    double GetTotalPositionLimit() const { return total_position_limit; }
    double GetDailyMaxLoss() const { return daily_max_loss; }
    double GetSingleStockStopLoss() const { return single_stock_stop_loss; }
    double GetSingleStockTakeProfit() const { return single_stock_take_profit; }
    // 收益计算参数的Getter
    double GetRiskFreeRate() const { return risk_free_rate; }
    void ReadConfigFromFile(const std::string& filename)
    {
        // 实现从JSON文件读取配置的逻辑
	}
};

class TransactionCostConfiger
{
private:
    double commission_rate = 0.0003;     // 佣金费率 | 万分之三（双边）
    double min_commission = 5.0;         // 最低佣金 | 每笔订单最低5元（A股规则）
    double stamp_duty_rate = 0.001;      // 印花税率 | 千分之一（仅卖出收取）
    double transfer_fee_rate = 0.00002;  // 过户费率 | 十万分之二（双边）
    double buy_slippage_rate = 0.001;    // 买入滑点率
    double sell_slippage_rate = 0.0015;  // 卖出滑点率（通常比买入高）
    double market_impact_coeff = 0.1;    // 市场冲击系数 | 大额交易冲击成本
public:
    // Getter方法
    double GetCommissionRate() const { return commission_rate; }
    double GetMinCommission() const { return min_commission; }
    double GetStampDutyRate() const { return stamp_duty_rate; }
    double GetTransferFeeRate() const { return transfer_fee_rate; }
    double GetBuySlippageRate() const { return buy_slippage_rate; }
    double GetSellSlippageRate() const { return sell_slippage_rate; }
    double GetMarketImpactCoeff() const { return market_impact_coeff; }

    void ReadConfigFromFile(const std::string& filename)
    {
        // 实现从JSON文件读取配置的逻辑
    }
};

class Configer
{
private:
	DataLevelConfiger data_level_configer;
	static Configer configer_instance;
	static StrategyConfiger strategy_configer_instance;
    static TransactionCostConfiger transaction_configer_instance;
    static ParamSearchConfiger param_search_config_instance;
public:
	static DataLevelConfiger& GetDataLevelConfiger() { return configer_instance.data_level_configer; }
    static StrategyConfiger& GetStrategyConfiger() { return strategy_configer_instance; }
	static TransactionCostConfiger& GetTransactionCostConfiger() { return transaction_configer_instance; }
    static ParamSearchConfiger& GetParamSearchConfig() { return param_search_config_instance; }
};

