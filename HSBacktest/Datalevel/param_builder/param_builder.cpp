#include "param_builder.h"
#include "../../MyLog/Logger.h"
#include <algorithm>
#include <numeric>
#include <cmath>

// ===== 便捷方法：默认随机模式 =====
void ParamBuilder::BuildParamNet(std::vector<AdjustParam>& adjustParams, int randomSamples)
{
    ParamSearchConfiger cfg;
    cfg.SetMode(ParamSearchConfiger::SearchMode::RANDOM);
    cfg.SetRandomSamples(randomSamples);
    BuildParamNet(adjustParams, cfg);
}

// ===== 主入口 =====
void ParamBuilder::BuildParamNet(std::vector<AdjustParam>& adjustParams,
                                  const ParamSearchConfiger& config)
{
    adjustParams.clear();

    switch (config.GetMode())
    {
    case ParamSearchConfiger::SearchMode::GRID:
        BuildGrid(adjustParams, config);
        break;
    case ParamSearchConfiger::SearchMode::RANDOM:
        BuildRandom(adjustParams, config);
        break;
    case ParamSearchConfiger::SearchMode::SINGLE_FACTOR:
        BuildSingleFactor(adjustParams, config);
        break;
    }

    LOG_INFO("ParamBuilder: generated {} parameter combinations", adjustParams.size());
}

// ===== 网格搜索 =====
void ParamBuilder::BuildGrid(std::vector<AdjustParam>& out, const ParamSearchConfiger& cfg)
{
    //为每个因子生成步长序列
    auto steps0 = GenerateSteps(cfg.GetMomentumWeightMin(),  cfg.GetMomentumWeightMax(),  cfg.GetGridStep());
    auto steps1 = GenerateSteps(cfg.GetTurnoverWeightMin(),  cfg.GetTurnoverWeightMax(),  cfg.GetGridStep());
    auto steps2 = GenerateSteps(cfg.GetVolatilityWeightMin(), cfg.GetVolatilityWeightMax(), cfg.GetGridStep());
    auto steps3 = GenerateSteps(cfg.GetMcapWeightMin(),      cfg.GetMcapWeightMax(),      cfg.GetGridStep());
    auto steps4 = GenerateSteps(cfg.GetEpWeightMin(),        cfg.GetEpWeightMax(),        cfg.GetGridStep());

    for (auto w0 : steps0)
    for (auto w1 : steps1)
    for (auto w2 : steps2)
    for (auto w3 : steps3)
    for (auto w4 : steps4)
    {
        AdjustParam param;
        param.factor_weights = { w0, w1, w2, w3, w4 };

        if (cfg.GetNormalizeWeights())
            NormalizeWeights(param.factor_weights);

        //如果不允许零权重，跳过含零的组
        if (!cfg.GetAllowZeroWeight())
        {
            bool hasZero = false;
            for (auto w : param.factor_weights)
                if (w < 1e-10) { hasZero = true; break; }
            if (hasZero) continue;
        }

        //为每个权重组合生成所有top_n组合
        for (int tn : cfg.GetTopNCandidates())
        {
            param.top_n = tn;
            out.push_back(param);
        }
    }
}

// ===== 随机采样（Dirichlet分布）=====
void ParamBuilder::BuildRandom(std::vector<AdjustParam>& out, const ParamSearchConfiger& cfg)
{
    std::mt19937 rng(std::random_device{}());

    constexpr int MAX_ZERO_WEIGHT_RETRIES = 100;

    for (int i = 0; i < cfg.GetRandomSamples(); ++i)
    {
        AdjustParam param;
        param.factor_weights = SampleDirichlet(rng, cfg);

        if (cfg.GetNormalizeWeights())
            NormalizeWeights(param.factor_weights);

        if (!cfg.GetAllowZeroWeight())
        {
            int retries = 0;
            bool hasZero = false;
            for (auto w : param.factor_weights)
                if (w < 1e-10) { hasZero = true; break; }
            while (hasZero && retries < MAX_ZERO_WEIGHT_RETRIES) {
                param.factor_weights = SampleDirichlet(rng, cfg);
                if (cfg.GetNormalizeWeights())
                    NormalizeWeights(param.factor_weights);
                hasZero = false;
                for (auto w : param.factor_weights)
                    if (w < 1e-10) { hasZero = true; break; }
                ++retries;
            }
            if (hasZero) {
                LOG_WARN("ParamBuilder::BuildRandom - exceeded max retries ({}), allowing zero-weight sample", MAX_ZERO_WEIGHT_RETRIES);
            }
        }

        //top_n：随机选取或全部候选
        const auto& topNCandidates = cfg.GetTopNCandidates();
        if (cfg.GetRandomTopN() && !topNCandidates.empty())
        {
            std::uniform_int_distribution<size_t> dist(0, topNCandidates.size() - 1);
            param.top_n = topNCandidates[dist(rng)];
            out.push_back(param);
        }
        else
        {
            //为每个权重组合生成所有top_n候选
            for (int tn : topNCandidates)
            {
                param.top_n = tn;
                out.push_back(param);
            }
        }
    }
}

// ===== 单因子扫描 =====
void ParamBuilder::BuildSingleFactor(std::vector<AdjustParam>& out, const ParamSearchConfiger& cfg)
{
    //默认基准权重（各因子范围的中点）
    std::array<double, FACTOR_NUM> baseline = {
        (cfg.GetMomentumWeightMin() + cfg.GetMomentumWeightMax()) / 2.0,
        (cfg.GetTurnoverWeightMin() + cfg.GetTurnoverWeightMax()) / 2.0,
        (cfg.GetVolatilityWeightMin() + cfg.GetVolatilityWeightMax()) / 2.0,
        (cfg.GetMcapWeightMin() + cfg.GetMcapWeightMax()) / 2.0,
        (cfg.GetEpWeightMin() + cfg.GetEpWeightMax()) / 2.0
    };
    if (cfg.GetNormalizeWeights()) NormalizeWeights(baseline);

    //依次扫描每个因子
    for (int f = 0; f < FACTOR_NUM; ++f)
    {
        auto range = GetWeightRange(f, cfg);
        auto steps = GenerateSteps(range.first, range.second, cfg.GetGridStep());

        for (double w : steps)
        {
            AdjustParam param;
            param.factor_weights = baseline;
            param.factor_weights[f] = w;

            if (cfg.GetNormalizeWeights())
                NormalizeWeights(param.factor_weights);

            for (int tn : cfg.GetTopNCandidates())
            {
                param.top_n = tn;
                out.push_back(param);
            }
        }
    }
}

// ===== 辅助方法 =====
std::pair<double, double> ParamBuilder::GetWeightRange(int factorIdx, const ParamSearchConfiger& cfg)
{
    switch (factorIdx)
    {
    case 0: return { cfg.GetMomentumWeightMin(),  cfg.GetMomentumWeightMax() };
    case 1: return { cfg.GetTurnoverWeightMin(),  cfg.GetTurnoverWeightMax() };
    case 2: return { cfg.GetVolatilityWeightMin(), cfg.GetVolatilityWeightMax() };
    case 3: return { cfg.GetMcapWeightMin(),      cfg.GetMcapWeightMax() };
    case 4: return { cfg.GetEpWeightMin(),        cfg.GetEpWeightMax() };
    default: return { 0.0, 1.0 };
    }
}

std::vector<double> ParamBuilder::GenerateSteps(double minVal, double maxVal, double step)
{
    std::vector<double> result;
    if (step <= 0.0) return { minVal };
    for (double v = minVal; v <= maxVal + 1e-10; v += step)
        result.push_back(v);
    return result;
}

void ParamBuilder::NormalizeWeights(std::array<double, FACTOR_NUM>& weights)
{
    double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
    if (sum > 1e-10)
        for (auto& w : weights) w /= sum;
    else
        std::fill(weights.begin(), weights.end(), 1.0 / FACTOR_NUM);
}

std::array<double, FACTOR_NUM> ParamBuilder::SampleDirichlet(
    std::mt19937& rng, const ParamSearchConfiger& cfg)
{
    std::array<double, FACTOR_NUM> result{};
    std::array<std::pair<double, double>, FACTOR_NUM> ranges = {
        GetWeightRange(0, cfg), GetWeightRange(1, cfg),
        GetWeightRange(2, cfg), GetWeightRange(3, cfg),
        GetWeightRange(4, cfg)
    };

    //用Gamma分布生成Dirichlet样本
    std::gamma_distribution<double> gamma(1.0, 1.0);
    double sum = 0.0;
    for (int i = 0; i < FACTOR_NUM; ++i)
    {
        result[i] = gamma(rng);
        sum += result[i];
    }
    //归一化后缩放到各因子[min, max]范围
    for (int i = 0; i < FACTOR_NUM; ++i)
    {
        result[i] /= sum;
        result[i] = ranges[i].first + result[i] * (ranges[i].second - ranges[i].first);
    }

    return result;
}
