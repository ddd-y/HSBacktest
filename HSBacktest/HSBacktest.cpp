// HSBacktest.cpp: 定义应用程序的入口点。
//

#include "HSBacktest.h"
#include"MyLog/Logger.h"
using namespace std;

int main()
{
	HSBacktest::Logger::getInstance().init("./HSBacktest.log", spdlog::level::info);
	return 0;
}
