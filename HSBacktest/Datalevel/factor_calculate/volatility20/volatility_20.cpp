#include "volatility_20.h"
#include"../../data_defs.h"
#include<cmath>
#include<array>

//不用价格有效性检查，在外部调用时，价格有效性检查已经完成
inline double volatility_20::calculate_volatility_20(const std::vector<StockDailyData>& daily_datas, const int date_index) const
{
    const int start_idx = date_index - 21; 
    const int end_idx = date_index - 1;

    // 步骤1：预分配固定大小array存储20个对数收益率
    // 20 = 21个交易日 - 1，固定长度无需动态扩容
    std::array<double, 20> daily_returns{}; 
    int ret_idx = 0; // 收益率数组索引

    // 步骤2：计算复权收盘价 + 填充对数收益率
    // 21个价格点产生20个收益率：从start_idx+1到end_idx
    double prev_price = daily_datas[start_idx].close * daily_datas[start_idx].adj_factor;

    // 价格有效性检查，避免除以0和log(0)产生NaN
    if (prev_price <= 0.0) {
        return 0.0;
    }

    for (int i = start_idx + 1; i <= end_idx; ++i) 
    {
        double curr_price = daily_datas[i].close * daily_datas[i].adj_factor;

        // 价格有效性检查
        /*if (curr_price <= 0.0) {
            return 0.0;
        }*/

        daily_returns[ret_idx++] = std::log(curr_price / prev_price); // 直接填充array
        prev_price = curr_price;
    }

    // 防御性检查：确保正好计算了20个收益率
    /*if (ret_idx != 20) {
        return 0.0;
    }*/

    // 步骤3：计算收益率均值
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
    double variance = std::max(variance_sum, 0.0) / 19;
    double std_dev = std::sqrt(variance);

    return std_dev;
}

void volatility_20::update_volatility_20(const std::vector<StockDailyData>& daily_datas, const std::vector<int>& rebalance_index)
{
    // 注意：为了调试，暂时移除OpenMP
    // #pragma omp parallel for schedule(static) 
    for (int i = 0; i < static_cast<int>(rebalance_index.size()); ++i) 
    {
        double volatility_20_value = calculate_volatility_20(daily_datas, rebalance_index[i]);
        // 确保索引不越界
        if (i < static_cast<int>(volatility_20s.size())) {
            volatility_20s[i] = volatility_20_value;
        }
    }
}