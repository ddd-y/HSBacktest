#include "configer.h"
#include <fstream>
#include "json.hpp"  // nlohmann/json 单头文件，放本目录即可

using json = nlohmann::json;

// ===== 静态成员定义 =====
Configer Configer::configer_instance;
StrategyConfiger Configer::strategy_configer_instance;
TransactionCostConfiger Configer::transaction_configer_instance;
ParamSearchConfiger Configer::param_search_config_instance;

// 系统级配置默认值
std::vector<std::string> Configer::stock_data_files = { "stock_data.csv" };
bool Configer::use_mpi = false;
std::string Configer::log_path = "logs/backtest.log";
double Configer::init_capital = 1000000.0;  // 默认 100 万

// ===== 辅助：安全读 JSON 字段 =====
namespace {
    template<typename T>
    void read_if_exists(const json& j, const char* key, T& out) {
        if (j.contains(key)) out = j[key].get<T>();
    }
}

// ===== ParamSearchConfiger =====
void ParamSearchConfiger::ReadConfigFromFile(const std::string& filename)
{
    std::ifstream ifs(filename);
    if (!ifs.is_open()) return;

    json j;
    ifs >> j;
    if (!j.contains("param_search")) return;

    auto& ps = j["param_search"];

    if (ps.contains("mode")) {
        std::string m = ps["mode"];
        if (m == "GRID")          mode = SearchMode::GRID;
        else if (m == "RANDOM")   mode = SearchMode::RANDOM;
        else if (m == "SINGLE_FACTOR") mode = SearchMode::SINGLE_FACTOR;
    }

    // 因子权重范围 —— 从 JSON 数组读取（名字可选，顺序 = 注册表顺序）
    if (ps.contains("factors") && ps["factors"].is_array()) {
        int idx = 0;
        for (const auto& f : ps["factors"]) {
            if (idx >= (int)weight_mins.size()) break;
            if (f.is_object()) {
                read_if_exists(f, "min", weight_mins[idx]);
                read_if_exists(f, "max", weight_maxs[idx]);
            }
            ++idx;
        }
    }

    read_if_exists(ps, "grid_step",             grid_step);
    read_if_exists(ps, "random_samples",        random_samples);
    read_if_exists(ps, "normalize_weights",     normalize_weights);
    read_if_exists(ps, "allow_zero_weight",     allow_zero_weight);
    read_if_exists(ps, "random_top_n",          random_top_n);
    read_if_exists(ps, "seed",                  seed);

    if (ps.contains("top_n_candidates") && ps["top_n_candidates"].is_array()) {
        top_n_candidates.clear();
        for (const auto& v : ps["top_n_candidates"])
            top_n_candidates.push_back(v.get<int>());
    }
}

// ===== StrategyConfiger =====
void StrategyConfiger::ReadConfigFromFile(const std::string& filename)
{
    std::ifstream ifs(filename);
    if (!ifs.is_open()) return;

    json j;
    ifs >> j;
    if (!j.contains("strategy")) return;

    auto& s = j["strategy"];

    read_if_exists(s, "hold_days",                hold_days);
    read_if_exists(s, "top_n",                    top_n);
    read_if_exists(s, "min_stocks_per_industry",  min_stocks_per_industry);

    // 因子默认权重 —— 从 JSON 数组读取
    if (s.contains("factor_weights") && s["factor_weights"].is_array()) {
        int idx = 0;
        for (const auto& v : s["factor_weights"]) {
            if (idx >= (int)default_weights.size()) break;
            default_weights[idx] = v.get<double>();
            ++idx;
        }
    }

    read_if_exists(s, "auto_normalize_weights",   auto_normalize_weights);
    read_if_exists(s, "single_position_limit",    single_position_limit);
    read_if_exists(s, "industry_position_limit",  industry_position_limit);
    read_if_exists(s, "single_stock_stop_loss",   single_stock_stop_loss);
    read_if_exists(s, "single_stock_take_profit", single_stock_take_profit);
    read_if_exists(s, "risk_free_rate",           risk_free_rate);
}

// ===== TransactionCostConfiger =====
void TransactionCostConfiger::ReadConfigFromFile(const std::string& filename)
{
    std::ifstream ifs(filename);
    if (!ifs.is_open()) return;

    json j;
    ifs >> j;
    if (!j.contains("transaction_cost")) return;

    auto& tc = j["transaction_cost"];

    read_if_exists(tc, "commission_rate",     commission_rate);
    read_if_exists(tc, "min_commission",      min_commission);
    read_if_exists(tc, "stamp_duty_rate",     stamp_duty_rate);
    read_if_exists(tc, "transfer_fee_rate",   transfer_fee_rate);
    read_if_exists(tc, "buy_slippage_rate",   buy_slippage_rate);
    read_if_exists(tc, "sell_slippage_rate",  sell_slippage_rate);
    read_if_exists(tc, "market_impact_coeff", market_impact_coeff);
}

// ===== Configer（顶层）=====
void Configer::LoadFromFile(const std::string& filename)
{
    // 各子模块加载
    param_search_config_instance.ReadConfigFromFile(filename);
    strategy_configer_instance.ReadConfigFromFile(filename);
    transaction_configer_instance.ReadConfigFromFile(filename);

    // 系统级配置
    std::ifstream ifs(filename);
    if (!ifs.is_open()) return;

    json j;
    ifs >> j;

    if (j.contains("data") && j["data"].contains("stock_files")) {
        stock_data_files.clear();
        for (const auto& v : j["data"]["stock_files"])
            stock_data_files.push_back(v.get<std::string>());
    }

    if (j.contains("system")) {
        read_if_exists(j["system"], "use_mpi",      use_mpi);
        read_if_exists(j["system"], "log_path",     log_path);
        read_if_exists(j["system"], "init_capital", init_capital);
    }
}
