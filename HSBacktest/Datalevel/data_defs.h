#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>

// ====================== 全局常量 ======================
constexpr int ANNUAL_TRADE_DAYS = 252;    // A股年均交易日数
constexpr int CACHE_LINE_SIZE = 64;       // CPU缓存行大小（用于内存对齐优化）

// ==========================================
// 第一层：StockDailyData（回测引擎核心最小集）
// 职责：支撑回测引擎最基本的日频回测运转（缺了就转不动）
// 仅保留：时间、成交价、复权、交易状态 四类核心字段
// ==========================================
struct alignas(CACHE_LINE_SIZE) StockDailyData {
    int32_t trade_date = 0;              // 交易日期 | 格式：YYYYMMDD (如20250101)
    double close = 0.0;                   // 当日收盘价 | 原始价格（未复权）
    double open = 0.0;                    // 当日开盘价 | 原始价格（未复权）
    double adj_factor = 1.0;              // 复权因子 | 用于计算前复权/后复权价格
    int is_suspended = 0;                // 停牌状态 | 0=正常交易 1=全天停牌
    int is_delisted = 0;                 // 退市状态 | 0=正常上市 1=已退市
    int is_limit_up = 0;                 // 涨停状态 | 0=未涨停 1=涨停
    int is_limit_down = 0;               // 跌停状态 | 0=未跌停 1=跌停
};

// ==========================================
// 第二层：StockDailyExtendedData（扩展行情数据）
// 职责：扩展策略使用（日内止损、VWAP、技术指标计算）
// 非回测引擎核心必须，按需加载
// ==========================================
struct alignas(CACHE_LINE_SIZE) StockDailyExtendedData {
    double high = 0.0;                   // 当日最高价 | 原始价格（未复权）
    double low = 0.0;                    // 当日最低价 | 原始价格（未复权）
    double volume = 0.0;                 // 成交量 | 单位：股
    double amount = 0.0;                 // 成交额 | 单位：元
};

// ==========================================
// 第三层：StockDailyFinancialData（财务/公司行为/因子依赖数据）
// 职责：因子计算、分红拆股精细处理、基本面分析
// 非核心必须，仅因子策略/精细回测需要加载
// ==========================================
struct alignas(CACHE_LINE_SIZE) StockDailyFinancialData {
    // --- 公司行为数据（分红、拆股）---
    double cash_dividend = 0.0;          // 现金分红 | 单位：元/股
    double split_ratio = 1.0;            // 拆股/送股比例 | 如10送10=2.0

    // --- 股本数据（因子计算核心依赖）---
    int64_t total_shares = 0;            // 总股本 | 单位：股
    int64_t float_shares = 0;            // 流通股本 | 单位：股

    // --- 核心财务指标 ---
    double eps_ttm = 0.0;               // 每股收益TTM | 过去12个月滚动收益
    double pe_ttm = 0.0;                 // 市盈率TTM | 滚动估值指标
    double pb_lf = 0.0;                  // 市净率LF | 最新财报市净率
    double roe_ttm = 0.0;                // 净资产收益率TTM | 盈利能力指标
};

// ==========================================
// 第四层：StockDailyTransactionCost（交易成本数据）
// 职责：实盘级回测（佣金、税费、滑点、冲击成本）
// 注：工业级实践中，交易成本多为全局配置，无需按日存储
// ==========================================
struct alignas(CACHE_LINE_SIZE) StockDailyTransactionCost {
    double commission_rate = 0.0003;     // 佣金费率 | 默认万分之三
    double stamp_duty_rate = 0.001;      // 印花税率 | 默认千分之一（仅卖出收取）
    double transfer_fee_rate = 0.00002;  // 过户费率 | 默认十万分之二
    double slippage_rate = 0.001;        // 滑点率 | 模拟实际成交与挂单的偏差
    double market_impact_coeff = 0.0;    // 市场冲击系数 | 大额交易冲击成本
    double avg_daily_volume_20 = 0.0;    // 20日日均成交量 | 用于冲击成本计算
};



