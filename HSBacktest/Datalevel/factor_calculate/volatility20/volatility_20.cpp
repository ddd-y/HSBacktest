#include "volatility_20.h"
#include"../../data_defs.h"
#include<cmath>
#include<array>

inline double volatility_20::calculate_volatility_20(const std::vector<StockDailyData>& daily_datas, const int date_index) const
{
    // 时间窗口与momentum_20完全对齐：date_index-21 到 date_index-1 共20个交易日
    const int start_idx = date_index - 21;
    const int end_idx = date_index - 1;

    // 步骤1：预分配固定大小array存储19个对数收益率（栈内存，比vector高效）
    // 19 = 20个交易日 - 1，固定长度无需动态扩容
    std::array<double, 19> daily_returns{}; // 初始化为0，贴合代码库初始化风格
    int ret_idx = 0; // 收益率数组索引

    // 步骤2：计算复权收盘价 + 填充对数收益率
    double prev_price = daily_datas[start_idx].close * daily_datas[start_idx].adj_factor;
    for (int i = start_idx + 1; i <= end_idx; ++i)
    {
        double curr_price = daily_datas[i].close * daily_datas[i].adj_factor;
        daily_returns[ret_idx++] = std::log(curr_price / prev_price); // 直接填充array
        prev_price = curr_price;
    }

    // 步骤3：计算收益率均值（复用原始逻辑，仅容器换array）
    double ret_mean = 0.0;
    for (double ret : daily_returns)
    {
        ret_mean += ret;
    }
    ret_mean /= daily_returns.size();

    // 步骤4：计算无偏标准差（波动率）
    double variance_sum = 0.0;
    for (double ret : daily_returns)
    {
        variance_sum += std::pow(ret - ret_mean, 2);
    }
    // 无偏估计：除以n-1（19-1=18），和原始逻辑一致
    double std_dev = std::sqrt(variance_sum / (daily_returns.size() - 1));

    return std_dev;
}

void volatility_20::update_volatility_20(const std::vector<StockDailyData>& daily_datas, const std::vector<int>& rebalance_index)
{
#pragma omp parallel for schedule(static) 
    for (int i = 0; i < rebalance_index.size(); ++i) 
    {
        double volatility_20_value = calculate_volatility_20(daily_datas, rebalance_index[i]);
        volatility_20s[i] = volatility_20_value;
    }
}