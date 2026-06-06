#!/usr/bin/env python3
"""
HSBacktest 测试数据生成器
生成符合项目格式的股票 K 线 CSV 数据 + config.json

用法（在 Windows 终端或 WSL 里均可运行）:
    python generate_test_data.py                    # 默认 5 只股票, 504 天
    python generate_test_data.py --stocks 10 --days 756 --output ./data
"""

import argparse
import math
import os
import random
import json
from datetime import date, timedelta

# ============================================================
# 配置
# ============================================================
DEFAULT_STOCKS = 5
DEFAULT_DAYS = 504          # ~2 年交易日 (252×2)
DEFAULT_OUTPUT = "./data"
PRE_EXTRA_DAYS = 21         # 动量因子需要的前置天数
HOLD_DAYS = 20              # 默认调仓周期

# ============================================================
# 交易日历生成
# ============================================================
def generate_trading_dates(num_days: int, start: date = date(2023, 1, 1)) -> list[int]:
    """生成连续的交易日列表 (YYYYMMDD), 跳过周末"""
    dates = []
    current = start
    while len(dates) < num_days:
        if current.weekday() < 5:
            dates.append(int(current.strftime("%Y%m%d")))
        current += timedelta(days=1)
    return dates


# ============================================================
# 价格生成 (几何布朗运动)
# ============================================================
def generate_prices(num_days: int, start_price: float = 10.0,
                    drift: float = 0.0003, vol: float = 0.015,
                    seed: int = 42) -> tuple[list[float], list[float]]:
    """
    返回 (close_prices, open_prices)
    drift = 日均收益率 (0.0003 ≈ 7.5% 年化)
    vol   = 日波动率
    """
    rng = random.Random(seed)
    closes = [start_price]
    for _ in range(num_days - 1):
        ret = rng.gauss(drift, vol)
        closes.append(closes[-1] * (1.0 + ret))

    opens = [c * (1.0 + rng.gauss(0, 0.002)) for c in closes]
    return closes, opens


# ============================================================
# 单只股票的 CSV 生成
# ============================================================
def write_stock_files(code: str, dates: list[int],
                      closes: list[float], opens: list[float],
                      industry: int, output_dir: str):
    """生成 {code}_daily.csv / _extended.csv / _financial.csv 三个文件"""

    n = len(dates)

    # ---- _daily.csv ----
    with open(os.path.join(output_dir, f"{code}_daily.csv"), "w") as f:
        f.write("trade_date,close,open,adj_factor,industry_code,"
                "is_suspended,is_delisted,is_limit_up,is_limit_down\n")
        for i in range(n):
            adj = 1.0
            suspended = 0
            delisted = 0
            limit_up = 0
            limit_down = 0

            # 偶尔制造一些特殊状态让回测更真实
            if i < PRE_EXTRA_DAYS:
                pass
            elif i == n - 5:
                suspended = 1
            elif i == n - 1:
                delisted = 0

            f.write(f"{dates[i]},{closes[i]:.2f},{opens[i]:.2f},{adj:.1f},"
                    f"{industry},{suspended},{delisted},{limit_up},{limit_down}\n")

    # ---- _daily_extended.csv ----
    with open(os.path.join(output_dir, f"{code}_daily_extended.csv"), "w") as f:
        f.write("trade_date,high,low,volume,amount\n")
        rng = random.Random(42 + int(code))
        for i in range(n):
            base = closes[i]
            high = base * (1.0 + abs(rng.gauss(0, 0.01)))
            low  = base * (1.0 - abs(rng.gauss(0, 0.01)))
            vol  = 1_000_000 + rng.randint(0, 5_000_000)
            amt  = vol * base
            f.write(f"{dates[i]},{high:.2f},{low:.2f},{vol:.0f},{amt:.0f}\n")

    # ---- _daily_financial.csv ----
    with open(os.path.join(output_dir, f"{code}_daily_financial.csv"), "w") as f:
        f.write("cash_dividend,split_ratio,total_shares,float_shares,"
                "eps_ttm,pe_ttm,pb_lf,roe_ttm\n")
        eps = 0.2 + int(code) * 0.05    # 匹配低价股
        pe = 15.0 + int(code) * 2.0
        for i in range(n):
            dividend = 0.0
            split = 1.0
            if i == n // 2:
                dividend = round(closes[i] * 0.02, 2)
            f.write(f"{dividend},{split},1000000000,800000000,"
                    f"{eps:.2f},{pe:.1f},2.5,0.12\n")


# ============================================================
# config.json 生成
# ============================================================
def write_config(stock_codes: list[str], output_dir: str, use_mpi: bool = False):
    config = {
        "data": {
            "stock_files": [os.path.join(output_dir, c) for c in stock_codes]
        },
        "system": {
            "use_mpi": use_mpi,
            "log_path": "logs/backtest.log"
        },
        "param_search": {
            "mode": "RANDOM",
            "random_samples": 100,
            "normalize_weights": True,
            "top_n_candidates": [20, 50],
            "random_top_n": True,
            "seed": 42
        },
        "strategy": {
            "hold_days": 20,
            "top_n": 50,
            "single_position_limit": 0.5
        }
    }
    with open(os.path.join(output_dir, "config.json"), "w") as f:
        json.dump(config, f, indent=2, ensure_ascii=False)


# ============================================================
# main
# ============================================================
def main():
    parser = argparse.ArgumentParser(description="HSBacktest 测试数据生成")
    parser.add_argument("--stocks", type=int, default=DEFAULT_STOCKS,
                        help=f"股票数量 (默认 {DEFAULT_STOCKS})")
    parser.add_argument("--days", type=int, default=DEFAULT_DAYS,
                        help=f"交易天数, 至少 {PRE_EXTRA_DAYS + HOLD_DAYS} (默认 {DEFAULT_DAYS})")
    parser.add_argument("--output", type=str, default=DEFAULT_OUTPUT,
                        help=f"输出目录 (默认 {DEFAULT_OUTPUT})")
    parser.add_argument("--mpi", action="store_true",
                        help="生成 MPI 模式的 config.json")
    args = parser.parse_args()

    if args.days < PRE_EXTRA_DAYS + HOLD_DAYS:
        print(f"⚠ 天数至少 {PRE_EXTRA_DAYS + HOLD_DAYS} "
              f"(PRE_EXTRA_DAYS={PRE_EXTRA_DAYS} + hold_days={HOLD_DAYS})")
        args.days = PRE_EXTRA_DAYS + HOLD_DAYS

    os.makedirs(args.output, exist_ok=True)
    os.makedirs("logs", exist_ok=True)

    dates = generate_trading_dates(args.days)
    stock_codes = [f"{i:06d}" for i in range(1, args.stocks + 1)]

    print(f"生成 {args.stocks} 只股票 x {args.days} 天 -> {args.output}/")
    for i, code in enumerate(stock_codes):
        closes, opens = generate_prices(
            args.days,
            start_price=8.0 + i * 2.0,   # 低价，确保 10000 本金能买到至少一手
            seed=100 + i
        )
        industry = 1000 + (i % 3) * 1000  # 3 个行业轮换
        write_stock_files(code, dates, closes, opens, industry, args.output)
        print(f"  OK {code}  (行业 {industry}, 起始价 {closes[0]:.1f})")

    write_config(stock_codes, args.output, use_mpi=args.mpi)
    print(f"  OK config.json")
    print()
    print(f"完成。运行方式:")
    print(f"  cd {args.output}")
    print(f"  ../build/HSBacktest/HSBacktest          # 单机")
    if args.mpi:
        print(f"  mpirun -np 4 ../build/HSBacktest/HSBacktest  # MPI")


if __name__ == "__main__":
    main()
