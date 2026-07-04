#pragma once
#include <vector>

// ==========================================
// 因子元数据 —— 加因子只改这一个 vector
// 顺序决定 factor_weights / GetValue 中的下标
// ==========================================
struct FactorMeta {
	const char* name;          // 因子名称（日志/调试用）
	double default_weight;     // 策略默认权重
	double weight_min;         // 参数搜索默认下限
	double weight_max;         // 参数搜索默认上限
};

inline const std::vector<FactorMeta>& GetFactorRegistry() {
	static const std::vector<FactorMeta> registry = {
		{"momentum_20",   0.2, 0.0, 1.0},
		{"turnover_20",   0.1, 0.0, 1.0},
		{"volatility_20", 0.1, 0.0, 1.0},
		{"log_mcap",      0.3, 0.0, 1.0},
		{"ep_ratio",      0.3, 0.0, 1.0},
	};
	return registry;
}

// 因子总数（自动从注册表推导，编译期常量）
constexpr int FACTOR_NUM = 5;
static_assert(FACTOR_NUM > 0, "Factor registry must not be empty");
