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

    // 前置校验：空 top_n_candidates 会导致所有模式静默零输出
    if (config.GetTopNCandidates().empty())
    {
        LOG_WARN("ParamBuilder::BuildParamNet - top_n_candidates is empty, no parameters will be generated");
        return;
    }

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

    // 网格组合数爆炸警告
    {
        long long estTotal = (long long)steps0.size() * steps1.size() * steps2.size()
                           * steps3.size() * steps4.size() * cfg.GetTopNCandidates().size();
        if (estTotal > GRID_EXPLOSION_WARN)
            LOG_WARN("ParamBuilder::BuildGrid - estimated {} total combinations (>{})", estTotal, (long long)GRID_EXPLOSION_WARN);
    }

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
    if (cfg.GetRandomSamples() <= 0)
    {
        LOG_WARN("ParamBuilder::BuildRandom - randomSamples is {} (<=0), no parameters generated", cfg.GetRandomSamples());
        return;
    }

    const auto& topNCandidates = cfg.GetTopNCandidates();
    if (topNCandidates.empty())
    {
        LOG_WARN("ParamBuilder::BuildRandom - top_n_candidates is empty, no parameters generated");
        return;
    }

    unsigned int s = cfg.GetSeed();
    std::mt19937 rng(s != 0 ? s : std::random_device{}());

    for (int i = 0; i < cfg.GetRandomSamples(); ++i)
    {
        AdjustParam param;
        param.factor_weights = SampleDirichlet(rng, cfg, cfg.GetNormalizeWeights());

        if (!cfg.GetAllowZeroWeight())
        {
            int retries = 0;
            bool hasZero = false;
            for (auto w : param.factor_weights)
                if (w < 1e-10) { hasZero = true; break; }
            while (hasZero && retries < MAX_ZERO_WEIGHT_RETRIES) {
                param.factor_weights = SampleDirichlet(rng, cfg, cfg.GetNormalizeWeights());
                hasZero = false;
                for (auto w : param.factor_weights)
                    if (w < 1e-10) { hasZero = true; break; }
                ++retries;
            }
            if (hasZero) {
                LOG_WARN("ParamBuilder::BuildRandom - exceeded max zero-weight retries ({}), allowing zero-weight sample", MAX_ZERO_WEIGHT_RETRIES);
            }
        }

        //top_n：随机选取或全部候选
        if (cfg.GetRandomTopN())
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

            // 零权重检查（与 BuildGrid/BuildRandom 行为一致）
            if (!cfg.GetAllowZeroWeight())
            {
                bool hasZero = false;
                for (auto v : param.factor_weights)
                    if (v < 1e-10) { hasZero = true; break; }
                if (hasZero) continue;
            }

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
    default:
        LOG_ERROR("ParamBuilder::GetWeightRange - invalid factorIdx {} (FACTOR_NUM={}), returning [0,1]", factorIdx, FACTOR_NUM);
        return { 0.0, 1.0 };
    }
}

std::vector<double> ParamBuilder::GenerateSteps(double minVal, double maxVal, double step)
{
    std::vector<double> result;
    if (step <= 0.0) return { minVal };
    if (minVal > maxVal)
    {
        LOG_WARN("ParamBuilder::GenerateSteps - minVal ({}) > maxVal ({}), using minVal only", minVal, maxVal);
        return { minVal };
    }
    int n = static_cast<int>(std::round((maxVal - minVal) / step)) + 1;
    result.reserve(n);
    for (int i = 0; i < n; ++i)
        result.push_back(minVal + i * step);
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
    std::mt19937& rng, const ParamSearchConfiger& cfg, bool normalize)
{
    std::array<std::pair<double, double>, FACTOR_NUM> ranges = {
        GetWeightRange(0, cfg), GetWeightRange(1, cfg),
        GetWeightRange(2, cfg), GetWeightRange(3, cfg),
        GetWeightRange(4, cfg)
    };

    std::gamma_distribution<double> gamma(1.0, 1.0);

    auto sample_raw = [&]() -> std::array<double, FACTOR_NUM> {
        std::array<double, FACTOR_NUM> result{};
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
    };

    if (!normalize)
    {
        return sample_raw();
    }

    // normalize=true: 保证最终归一化后仍在[min,max]范围内
    for (int retry = 0; retry < MAX_RANGE_RETRIES; ++retry)
    {
        auto result = sample_raw();

        // 归一化
        double sum = std::accumulate(result.begin(), result.end(), 0.0);
        if (sum > 1e-10)
        {
            for (auto& w : result) w /= sum;
        }
        else
        {
            std::fill(result.begin(), result.end(), 1.0 / FACTOR_NUM);
        }

        // 检查是否仍在[min,max]范围内
        bool inRange = true;
        for (int i = 0; i < FACTOR_NUM; ++i)
        {
            if (result[i] < ranges[i].first - 1e-10 || result[i] > ranges[i].second + 1e-10)
            {
                inRange = false;
                break;
            }
        }
        if (inRange) return result;
    }

    // 回退：范围中点归一化
    LOG_WARN("ParamBuilder::SampleDirichlet - exceeded max range retries ({}), falling back to midpoints", MAX_RANGE_RETRIES);
    std::array<double, FACTOR_NUM> fallback{};
    for (int i = 0; i < FACTOR_NUM; ++i)
        fallback[i] = (ranges[i].first + ranges[i].second) / 2.0;
    NormalizeWeights(fallback);
    return fallback;
}
