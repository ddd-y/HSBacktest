#pragma once
#include<vector>
#include"momentnum20/momentum_20.h"
#include"epratio/ep_ratio.h"
#include"logmcap/log_mcap.h"
#include"turnover20/turnover_20.h"
#include"volatility20/volatility_20.h"

class FactorDatabase
{
private:
	momentum_20_data momentum_20_data_obj;
	ep_ratio_data ep_ratio_data_obj;
	log_mcap_data log_mcap_data_obj;
	volatility_20_data volatility_20_data_obj;
	turnover_20_data turnover_20_data_obj;
public:
	momentum_20_data& get_momentum_20_data() { return momentum_20_data_obj; }
	ep_ratio_data& get_ep_ratio_data() { return ep_ratio_data_obj; }
	log_mcap_data& get_log_mcap_data() { return log_mcap_data_obj; }
	volatility_20_data& get_volatility_20_data() { return volatility_20_data_obj; }
	turnover_20_data& get_turnover_20_data() { return turnover_20_data_obj; }

	FactorDatabase(int data_size) : 
		momentum_20_data_obj(data_size),
		ep_ratio_data_obj(data_size),
		log_mcap_data_obj(data_size),
		volatility_20_data_obj(data_size),
		turnover_20_data_obj(data_size)
	{}
};
