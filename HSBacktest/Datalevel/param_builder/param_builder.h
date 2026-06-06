#pragma once
#include<vector>
#include<array>
#include<random>
#include"../factor_calculate/factorbase.h"
#include"../../ConfigLvevl/configer.h"

//单组可调参数
struct AdjustParam 
{
    std::array<double,FACTOR_NUM> factor_weights{};
    int top_n = 50;
};

//参数生成器
class ParamBuilder
{
public:
    //根据配置构建参数网络
    static void BuildParamNet(std::vector<AdjustParam>& adjustParams,
                              const ParamSearchConfiger& config);

    //便捷方法：使用默认配置+指定采样数（随机模式）
    static void BuildParamNet(std::vector<AdjustParam>& adjustParams,
                              int randomSamples = 1000);

    // 常量
    static constexpr int MAX_ZERO_WEIGHT_RETRIES = 100;  // 拒绝零权重时的最大重试次数
    static constexpr int MAX_RANGE_RETRIES = 200;        // Dirichlet 归一化后范围约束重试上限
    static constexpr int GRID_EXPLOSION_WARN = 100000;   // 网格组合数超过此值时打 WARN

private:
    //各搜索模式实现
    static void BuildGrid(std::vector<AdjustParam>& out, const ParamSearchConfiger& cfg);
    static void BuildRandom(std::vector<AdjustParam>& out, const ParamSearchConfiger& cfg);
    static void BuildSingleFactor(std::vector<AdjustParam>& out, const ParamSearchConfiger& cfg);

    //获取某因子的min/max
    static std::pair<double, double> GetWeightRange(int factorIdx, const ParamSearchConfiger& cfg);

    //生成步长列表（如{0.0, 0.1, 0.2, ..., 1.0}）
    static std::vector<double> GenerateSteps(double minVal, double maxVal, double step);

    //归一化权重数组
    static void NormalizeWeights(std::array<double, FACTOR_NUM>& weights);

    //Dirichlet分布采样（用于随机模式）。normalize=true 时保证最终归一化后仍在[min,max]范围内
    static std::array<double, FACTOR_NUM> SampleDirichlet(
        std::mt19937& rng, const ParamSearchConfiger& cfg, bool normalize);
};
