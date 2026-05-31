// HSBacktest.cpp: 定义应用程序的入口点。
//
#include"test/dataleveltest.h"
#include"test/full_pipeline_test.h"
#include"test/multithread_test.h"
int main()
{
	//dataleveltest();
	//full_pipeline_test();
	multithread_runner_test();
}
