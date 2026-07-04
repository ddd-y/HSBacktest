#pragma once
#include<string>
#include<vector>
#include"../Datalevel/factor_calculate/factor_registry.h"

// ==========================================
// 参数搜索配置（用于并行参数优化）
// 因子权重范围通过下标访问，与注册表顺序一致
// ==========================================
class ParamSearchConfiger
{
public:
    enum class SearchMode { GRID, RANDOM, SINGLE_FACTOR };

private:
    SearchMode mode = SearchMode::RANDOM;

    //各因子权重范围（下标 = 注册表顺序）
    std::vector<double> weight_mins;
    std::vector<double> weight_maxs;

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

    //RNG种子（0=使用random_device随机种子）
    unsigned int seed = 0;

    // 从注册表初始化默认值
    void InitFromRegistry() {
        const auto& reg = GetFactorRegistry();
        weight_mins.resize(reg.size());
        weight_maxs.resize(reg.size());
        for (size_t i = 0; i < reg.size(); ++i) {
            weight_mins[i] = reg[i].weight_min;
            weight_maxs[i] = reg[i].weight_max;
        }
    }

public:
    ParamSearchConfiger() { InitFromRegistry(); }

    // Getter / Setter
    SearchMode GetMode() const { return mode; }
    void SetMode(SearchMode m) { mode = m; }

    // 因子权重范围 —— 通用接口（下标 = 注册表顺序）
    double GetWeightMin(int idx) const { return (idx >= 0 && idx < (int)weight_mins.size()) ? weight_mins[idx] : 0.0; }
    void   SetWeightMin(int idx, double v) { if (idx >= 0 && idx < (int)weight_mins.size()) weight_mins[idx] = v; }
    double GetWeightMax(int idx) const { return (idx >= 0 && idx < (int)weight_maxs.size()) ? weight_maxs[idx] : 1.0; }
    void   SetWeightMax(int idx, double v) { if (idx >= 0 && idx < (int)weight_maxs.size()) weight_maxs[idx] = v; }
    int    GetFactorCount() const { return static_cast<int>(weight_mins.size()); }

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

    unsigned int GetSeed() const { return seed; }
    void SetSeed(unsigned int v) { seed = v; }

    void ReadConfigFromFile(const std::string& filename);
};


class StrategyConfiger
{
    // ===== 调仓参数 =====
    int hold_days = 20;                  // 调仓周期（交易日）
    int top_n = 50;                      // 选股数量
    int min_stocks_per_industry = 1;      // 每个行业最少选股数（行业中性化选股）

    // ===== 因子默认权重（下标 = 注册表顺序，从 registry 初始化）=====
    std::vector<double> default_weights;
    // 权重自动归一化开关
    bool auto_normalize_weights = true;

    // ===== 完整风控规则 =====
    double single_position_limit = 0.5;  // 单票仓位上限
    double industry_position_limit = 0.2; // 单行业仓位上限
    double single_stock_stop_loss = 0.1;  // 单只股票止损比例（10%）
    double single_stock_take_profit = 0.3; // 单只股票止盈比例（30%）

    // ===== 收益计算参数 =====
    double risk_free_rate = 0.03;         // 无风险利率（年化3%）

    void InitFromRegistry() {
        const auto& reg = GetFactorRegistry();
        default_weights.resize(reg.size());
        for (size_t i = 0; i < reg.size(); ++i)
            default_weights[i] = reg[i].default_weight;
    }

public:
    StrategyConfiger() { InitFromRegistry(); }

    int GetHoldDays() const { return hold_days; }
    int GetTopN() const { return top_n; }
    int GetMinStocksPerIndustry() const { return min_stocks_per_industry; }

    // 因子权重 —— 通用接口（下标 = 注册表顺序）
    double GetDefaultWeight(int idx) const { return (idx >= 0 && idx < (int)default_weights.size()) ? default_weights[idx] : 0.0; }
    void   SetDefaultWeight(int idx, double v) { if (idx >= 0 && idx < (int)default_weights.size()) default_weights[idx] = v; }
    int    GetFactorCount() const { return static_cast<int>(default_weights.size()); }
    bool   GetAutoNormalizeWeights() const { return auto_normalize_weights; }

    // 风控规则的 Getter / Setter
    double GetSinglePositionLimit() const { return single_position_limit; }
    void SetSinglePositionLimit(double v) { single_position_limit = v; }
    double GetIndustryPositionLimit() const { return industry_position_limit; }
    void SetIndustryPositionLimit(double v) { industry_position_limit = v; }
    double GetSingleStockStopLoss() const { return single_stock_stop_loss; }
    void SetSingleStockStopLoss(double v) { single_stock_stop_loss = v; }
    double GetSingleStockTakeProfit() const { return single_stock_take_profit; }
    void SetSingleStockTakeProfit(double v) { single_stock_take_profit = v; }
    // 收益计算参数的Getter
    double GetRiskFreeRate() const { return risk_free_rate; }

    void ReadConfigFromFile(const std::string& filename);
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

    void ReadConfigFromFile(const std::string& filename);
};

class Configer
{
private:
	static Configer configer_instance;
	static StrategyConfiger strategy_configer_instance;
    static TransactionCostConfiger transaction_configer_instance;
    static ParamSearchConfiger param_search_config_instance;

    // ===== 系统级配置 =====
    static std::vector<std::string> stock_data_files;
    static bool use_mpi;
    static std::string log_path;
    static double init_capital;

public:
    static StrategyConfiger& GetStrategyConfiger() { return strategy_configer_instance; }
	static TransactionCostConfiger& GetTransactionCostConfiger() { return transaction_configer_instance; }
    static ParamSearchConfiger& GetParamSearchConfig() { return param_search_config_instance; }

    // 系统配置 Getter / Setter
    static const std::vector<std::string>& GetStockDataFiles() { return stock_data_files; }
    static void SetStockDataFiles(const std::vector<std::string>& v) { stock_data_files = v; }

    static bool GetUseMpi() { return use_mpi; }
    static void SetUseMpi(bool v) { use_mpi = v; }

    static const std::string& GetLogPath() { return log_path; }
    static void SetLogPath(const std::string& v) { log_path = v; }

    static double GetInitCapital() { return init_capital; }
    static void SetInitCapital(double v) { init_capital = v; }

    // 从 JSON 配置文件加载所有配置
    static void LoadFromFile(const std::string& filename);
};
