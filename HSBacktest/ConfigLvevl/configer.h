#pragma once
class Configer
{
private:
	DataLevelConfiger data_level_configer;
	static Configer configer_instance;
public:
	static DataLevelConfiger& GetDataLevelConfiger() { return configer_instance.data_level_configer; }
};

class DataLevelConfiger 
{
public:
    int GetChangeDuration();
};

//暂时用不到，后边再说
struct StrategyConfiger
{
};

struct TransactionCostConfiger
{
    double commission_rate = 0.0003;     // 佣金费率 | 默认万分之三
    double stamp_duty_rate = 0.001;      // 印花税率 | 默认千分之一（仅卖出收取）
    double transfer_fee_rate = 0.00002;  // 过户费率 | 默认十万分之二
    double slippage_rate = 0.001;        // 滑点率 | 模拟实际成交与挂单的偏差
    double market_impact_coeff = 0.0;    // 市场冲击系数 | 大额交易冲击成本
    double avg_daily_volume_20 = 0.0;    // 20日日均成交量 | 用于冲击成本计算
};