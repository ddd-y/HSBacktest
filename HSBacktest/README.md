# HSBacktest — A-share Multi-Factor Quantitative Backtesting Engine

C++ 高性能多因子量化回测系统，支持 **MPI 多机工作窃取 + OpenMP 多核并行**二级调度，覆盖因子计算、行业中性选股、真实交易成本模拟、参数搜索与稳健性分析全流程。

---

## 目录

- [项目简介](#项目简介)
- [快速开始](#快速开始)
- [config.json 配置说明](#configjson-配置说明)
  - [完整配置示例](#完整配置示例)
  - [字段说明](#字段说明)
- [因子系统：新增/修改因子](#因子系统新增修改因子)
  - [因子系统架构](#因子系统架构)
  - [新增一个因子（6 步）](#新增一个因子6-步)
  - [修改/删除现有因子](#修改删除现有因子)

---

## 项目简介

HSBacktest 是一个面向 A 股市场的多因子量化回测引擎，用于验证和优化股票交易策略。

**核心流程：** 加载历史日频行情 → 计算五大类因子（动量、波动率、换手率、市值、盈利收益率）→ 每个调仓日做横截面 Z-score 标准化 → 加权合成复合得分 → 行业中性选股 → 模拟交易（含佣金、印花税、滑点、涨跌停限制、公司行为处理）→ 输出年化收益、夏普比率、最大回撤等绩效指标 → 对参数空间做稳健性分析。

**技术亮点：**

| 特性 | 说明 |
|---|---|
| MPI 去中心化工作窃取 | 多机对等参与，随机 victim 窃取半数任务，`MPI_Isend`/`MPI_Iprobe` 非阻塞通信防死锁，分布式共识终止检测 |
| OpenMP 引擎池复用 | 预创建 BacktestEngine 池，整个 work-stealing 生命周期内反复 `ReInitialize` 复用，零分配开销 |
| 真实 A 股交易模型 | 最低佣金 ¥5、卖出 0.1% 印花税、过户费、滑点、100 股取整、涨跌停/停牌/退市过滤、仓位上限 |
| 参数搜索与稳健性分析 | 网格搜索 / Dirichlet 随机采样 / 单因子扫描，自动区分"平台型"与"尖峰型"参数区域 |
| 可扩展因子注册表 | 新增因子只需添加元数据 + 计算器，所有数组、循环、参数网格自动适配 |

---

## 快速开始

### 环境要求

- **编译器：** MSVC 2022 / GCC 11+ / Clang 15+（需支持 C++17/20）
- **CMake：** ≥ 3.10
- **MPI：** OpenMPI（仅多机模式需要）
- **依赖：** [spdlog](https://github.com/gabime/spdlog) + fmt（日志）、[nlohmann/json](https://github.com/nlohmann/json)（配置解析）、[csv.h](https://github.com/ben-strasser/fast-cpp-csv-parser)（CSV 读取）——均通过 FetchContent 或单头文件引入

### 构建

```bash
# 单机版（无需 MPI）
cmake -B out/build -DCMAKE_BUILD_TYPE=Release
cmake --build out/build --config Release

# 多机版（需要 OpenMPI）
cmake -B out/build -DCMAKE_BUILD_TYPE=Release -DUSE_MPI=ON
cmake --build out/build --config Release
```

### 运行

```bash
# 单机模式
./out/build/Release/HSBacktest

# MPI 多机模式（N 个进程，可跨机器）
mpirun -np N ./out/build/Release/HSBacktest
```

程序启动时自动读取工作目录下的 `config.json`，按配置加载数据并执行回测。

### 数据格式

每只股票需要 **三个** CSV 文件，以股票代码命名 + 对应后缀。例如股票代码为 `000001` 时需要：

```
data/000001_daily.csv           ← 核心日频数据（必需）
data/000001_daily_extended.csv  ← 扩展行情数据（必需）
data/000001_daily_financial.csv ← 财务及公司行为数据（必需）
```

程序启动时根据 `config.json` 中的 `data.stock_files` 列表，对每个股票代码**自动拼接三个后缀**加载对应 CSV。

> 例如配置 `"stock_files": ["data/000001", "data/000002"]`，程序会分别加载 `data/000001_daily.csv`、`data/000001_daily_extended.csv`、`data/000001_daily_financial.csv` 和 `data/000002_daily.csv`、`data/000002_daily_extended.csv`、`data/000002_daily_financial.csv`。

#### 文件一：`<代码>_daily.csv`（核心日频数据，9 列）

| 列名 | 类型 | 含义 |
|---|---|---|
| trade_date | int | 交易日，格式 YYYYMMDD |
| close | double | 收盘价（未复权） |
| open | double | 开盘价（未复权） |
| adj_factor | double | 复权因子 |
| industry_code | int | 申万行业代码，0=未分类 |
| is_suspended | int | 停牌标志，0=正常 1=停牌 |
| is_delisted | int | 退市标志，0=正常 1=已退市 |
| is_limit_up | int | 涨停标志，0=未涨停 1=涨停 |
| is_limit_down | int | 跌停标志，0=未跌停 1=跌停 |

#### 文件二：`<代码>_daily_extended.csv`（扩展行情数据，4 列）

| 列名 | 类型 | 含义 |
|---|---|---|
| high | double | 最高价（未复权） |
| low | double | 最低价（未复权） |
| volume | double | 成交量（股） |
| amount | double | 成交额（元） |

#### 文件三：`<代码>_daily_financial.csv`（财务及公司行为数据，8 列）

| 列名 | 类型 | 含义 |
|---|---|---|
| cash_dividend | double | 每股现金分红（元） |
| split_ratio | double | 拆送股比例（如 10 送 10 = 2.0，无则为 1.0） |
| total_shares | int64 | 总股本（股） |
| float_shares | int64 | 流通股本（股） |
| eps_ttm | double | 每股收益 TTM |
| pe_ttm | double | 市盈率 TTM |
| pb_lf | double | 市净率 LF |
| roe_ttm | double | 净资产收益率 TTM |

> 可用 `test/generate_test_data.py` 生成基于几何布朗运动的合成测试数据。

---

## config.json 配置说明

### 完整配置示例

```json
{
  "system": {
    "use_mpi": false,
    "log_path": "logs/backtest.log",
    "init_capital": 1000000.0
  },
  "data": {
    "stock_files": [
      "data/000001",
      "data/000002"
    ]
  },
  "strategy": {
    "hold_days": 20,
    "top_n": 30,
    "min_stocks_per_industry": 2,
    "factor_weights": [0.2, 0.1, 0.1, 0.3, 0.3],
    "auto_normalize_weights": true,
    "single_position_limit": 0.5,
    "industry_position_limit": 0.2,
    "single_stock_stop_loss": -0.10,
    "single_stock_take_profit": 0.30,
    "risk_free_rate": 0.03
  },
  "transaction_cost": {
    "commission_rate": 0.0003,
    "min_commission": 5.0,
    "stamp_duty_rate": 0.001,
    "transfer_fee_rate": 0.00002,
    "buy_slippage_rate": 0.0005,
    "sell_slippage_rate": 0.0005,
    "market_impact_coeff": 0.1
  },
  "param_search": {
    "mode": "RANDOM",
    "factors": [
      { "min": 0.0, "max": 1.0 },
      { "min": 0.0, "max": 1.0 },
      { "min": 0.0, "max": 1.0 },
      { "min": 0.0, "max": 1.0 },
      { "min": 0.0, "max": 1.0 }
    ],
    "random_samples": 1000,
    "normalize_weights": true,
    "allow_zero_weight": true,
    "top_n_candidates": [20, 30, 50, 80, 100],
    "random_top_n": false,
    "seed": 0
  }
}
```

### 字段说明

#### `system` — 系统级配置

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `use_mpi` | bool | false | 是否启用 MPI 多机模式 |
| `log_path` | string | logs/backtest.log | 日志输出路径 |
| `init_capital` | double | 1000000.0 | 初始资金（元） |

#### `data` — 数据配置

| 字段 | 类型 | 说明 |
|---|---|---|
| `stock_files` | string[] | 股票代码路径列表（不含后缀，程序自动拼接 `_daily.csv` / `_daily_extended.csv` / `_daily_financial.csv`） |

#### `strategy` — 策略配置

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `hold_days` | int | 20 | 调仓周期（交易日） |
| `top_n` | int | 30 | 每次选股数量 |
| `min_stocks_per_industry` | int | 2 | 行业中性化：每个行业最少选股数 |
| `factor_weights` | double[] | 注册表默认值 | 各因子权重，顺序对应注册表 |
| `auto_normalize_weights` | bool | true | 权重自动归一化（和=1） |
| `single_position_limit` | double | 0.5 | 单股仓位上限（占总资产比例） |
| `industry_position_limit` | double | 0.2 | 行业仓位上限 |
| `single_stock_stop_loss` | double | -0.10 | 单股止损线（如 -0.10 = -10%） |
| `single_stock_take_profit` | double | 0.30 | 单股止盈线 |
| `risk_free_rate` | double | 0.03 | 无风险利率（用于夏普比率） |

#### `transaction_cost` — 交易成本配置

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `commission_rate` | double | 0.0003 | 佣金费率 |
| `min_commission` | double | 5.0 | 最低佣金（元/笔） |
| `stamp_duty_rate` | double | 0.001 | 印花税率（仅卖出） |
| `transfer_fee_rate` | double | 0.00002 | 过户费率 |
| `buy_slippage_rate` | double | 0.0005 | 买入滑点比例 |
| `sell_slippage_rate` | double | 0.0005 | 卖出滑点比例 |
| `market_impact_coeff` | double | 0.1 | 市场冲击系数（量越大滑点越大） |

#### `param_search` — 参数搜索配置

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `mode` | string | RANDOM | `GRID` 网格搜索 / `RANDOM` Dirichlet 采样 / `SINGLE_FACTOR` 单因子扫描 |
| `factors` | object[] | 注册表默认 | 每个因子的 `min` / `max` 搜索范围，顺序对应注册表 |
| `random_samples` | int | 1000 | RANDOM 模式的采样数量 |
| `normalize_weights` | bool | true | 采样后权重归一化 |
| `allow_zero_weight` | bool | true | 是否允许某些因子权重为 0 |
| `top_n_candidates` | int[] | [20,30,50,80,100] | 需要测试的选股数量候选值 |
| `random_top_n` | bool | false | RANDOM 模式下 top_n 是否也随机选 |
| `seed` | unsigned | 0 | 随机种子（0 = 使用 random_device） |

---

## 因子系统：新增/修改因子

### 因子系统架构

```
factor_registry.h     ← 因子注册表（名称、默认权重、搜索范围）
       │
       ▼
factor_database.h     ← 因子数据容器（每只股票一个 vector<double>）
       │
       ▼
factorbase.h/.cpp     ← 因子计算器聚合（持有所有因子计算器指针）
       │
       ▼
momentnum20/          ← 具体因子：数据类 + 计算器类
epratio/                模式：每个因子一个子目录，含 .h + .cpp
logmcap/
turnover20/
volatility20/
```

每条数据链路（以 momentum_20 为例）：

```
GlobalData::GetValue(i, idx)
  → factor_databases[i]->get_momentum_20_data().get_momentum_20(idx)
    → FactorDatabase::momentum_20_data_obj[idx]
      → 由 momentum_20::update_momentum_20() 在初始化阶段计算填充
```

### 新增一个因子（6 步）

以新增"20 日换手率标准差因子" `turnover_std_20` 为例：

#### 第 1 步：创建数据类和计算器

新建目录 `HSBacktest/Datalevel/factor_calculate/turnoverstd20/`，创建两个文件：

**`turnover_std_20.h`**

```cpp
#pragma once
#include <vector>

// 数据容器
class turnover_std_20_data {
    std::vector<double> values;  // 每个调仓日一个值
public:
    double get_turnover_std_20(int index) const { return values[index]; }
    std::vector<double>& get_values() { return values; }
    turnover_std_20_data(int size) : values(size, 0.0) {}
};

// 计算器
class StockDailyData;
class StockDailyExtendedData;
class turnover_std_20 {
    std::vector<double>& data_ref;
    double calculate(const std::vector<StockDailyExtendedData>&, int date_idx) const;
public:
    void update(const std::vector<StockDailyExtendedData>&, const std::vector<int>& rebalance_idx);
    turnover_std_20(turnover_std_20_data& d) : data_ref(d.get_values()) {}
};
```

**`turnover_std_20.cpp`**

```cpp
#include "turnover_std_20.h"
#include "../../data_defs.h"
#include <cmath>

double turnover_std_20::calculate(
    const std::vector<StockDailyExtendedData>& extended, int idx) const
{
    // 计算最近 20 个交易日换手率的标准差
    int start = idx - 20;
    double sum = 0.0, sq = 0.0;
    for (int i = start; i < idx; ++i) {
        double t = extended[i].turnover_rate;
        sum += t; sq += t * t;
    }
    double mean = sum / 20.0;
    return std::sqrt(sq / 20.0 - mean * mean);
}

void turnover_std_20::update(
    const std::vector<StockDailyExtendedData>& extended,
    const std::vector<int>& rebalance_idx)
{
    for (int i = 0; i < (int)rebalance_idx.size(); ++i)
        data_ref[i] = calculate(extended, rebalance_idx[i]);
}
```

#### 第 2 步：注册到 FactorDatabase

编辑 `Datalevel/factor_calculate/factor_database.h`：

```cpp
// 1. 添加 include
#include "turnoverstd20/turnover_std_20.h"

// 2. 添加成员变量
class FactorDatabase {
    turnover_std_20_data turnover_std_20_data_obj;  // ← 新增
public:
    // 3. 添加访问器
    turnover_std_20_data& get_turnover_std_20_data() {
        return turnover_std_20_data_obj;
    }

    // 4. 构造函数初始化列表中追加
    FactorDatabase(int data_size)
        : momentum_20_data_obj(data_size),
          // ... 原有成员 ...
          turnover_std_20_data_obj(data_size)  // ← 新增
    {}
};
```

#### 第 3 步：注册到 FactorBase

编辑 `Datalevel/factor_calculate/factorbase.h` 和 `factorbase.cpp`：

```cpp
// factorbase.h —— 添加前向声明、成员指针和 getter
class turnover_std_20;
class FactorBase {
    turnover_std_20* turnover_std_20_calculator;
public:
    turnover_std_20* get_turnover_std_20_calculator() const {
        return turnover_std_20_calculator;
    }
};

// factorbase.cpp —— 构造/析构/update 三个位置追加
FactorBase::FactorBase(FactorDatabase& db) {
    // ... 原有 ...
    turnover_std_20_calculator = new turnover_std_20(db.get_turnover_std_20_data());
}

void FactorBase::update_factors(...) {
    // ... 原有 ...
    turnover_std_20_calculator->update(extended_datas, rebalance_index);
}

FactorBase::~FactorBase() {
    // ... 原有 ...
    delete turnover_std_20_calculator;
}
```

#### 第 4 步：添加到因子注册表

编辑 `Datalevel/factor_calculate/factor_registry.h`：

```cpp
inline const std::vector<FactorMeta>& GetFactorRegistry() {
    static const std::vector<FactorMeta> registry = {
        {"momentum_20",      0.2, 0.0, 1.0},
        {"turnover_20",      0.1, 0.0, 1.0},
        {"volatility_20",    0.1, 0.0, 1.0},
        {"log_mcap",         0.3, 0.0, 1.0},
        {"ep_ratio",         0.3, 0.0, 1.0},
        {"turnover_std_20",  0.1, 0.0, 1.0},  // ← 新增
    };
    return registry;
}
```

将 `FACTOR_NUM` 从 `5` 改为 `6`：

```cpp
constexpr int FACTOR_NUM = 6;
```

> 注册表中的顺序决定了 `factor_weights`、`GetValue` 中各因子的下标，新增因子追加在末尾即可，不影响已有因子的下标。

#### 第 5 步：接入 GlobalData::GetValue

编辑 `Datalevel/Global_data.h`，在 `GetValue` 返回的数组末尾追加新因子：

```cpp
inline std::array<double, FACTOR_NUM> GetValue(int index_1, int index_2) {
    return std::array<double, FACTOR_NUM>({
        factor_databases[index_1]->get_momentum_20_data().get_momentum_20(index_2),
        factor_databases[index_1]->get_turnover_20_data().get_turnover_20(index_2),
        factor_databases[index_1]->get_volatility_20_data().get_volatility_20(index_2),
        factor_databases[index_1]->get_log_mcap_data().get_log_mcap(index_2),
        factor_databases[index_1]->get_ep_ratio_data().get_ep_ratio(index_2),
        factor_databases[index_1]->get_turnover_std_20_data().get_turnover_std_20(index_2),  // ← 新增
    });
}
```

#### 第 6 步：config.json 中配置新因子

```json
{
  "strategy": {
    "factor_weights": [0.2, 0.1, 0.1, 0.3, 0.2, 0.1]
  },
  "param_search": {
    "factors": [
      { "min": 0.0, "max": 1.0 },
      { "min": 0.0, "max": 1.0 },
      { "min": 0.0, "max": 1.0 },
      { "min": 0.0, "max": 1.0 },
      { "min": 0.0, "max": 1.0 },
      { "min": 0.0, "max": 1.0 }
    ]
  }
}
```

`factor_weights` 和 `factors` 数组的长度必须等于 `FACTOR_NUM`（6），顺序与注册表一致。

---

### 修改/删除现有因子

**修改因子计算逻辑**：只需修改对应 `xxx.cpp` 中的 `calculate_xxx` 函数，无需改动其他文件。

**修改因子默认权重或搜索范围**：编辑 `factor_registry.h` 中对应的 `FactorMeta` 条目。

**删除一个因子**：按照新增的 6 步反向操作 —— 从 `GetValue` 中移除那一行、从 `FactorBase` 中删除对应计算器指针和 new/delete/update 调用、从 `FactorDatabase` 中删除数据成员、从注册表中删除条目并将 `FACTOR_NUM` 减 1、最后更新 `config.json` 中 `factor_weights` 和 `factors` 数组长度。

> 删除因子会改变数组中后续因子的下标，务必同步修改 `config.json` 中的权重配置。
