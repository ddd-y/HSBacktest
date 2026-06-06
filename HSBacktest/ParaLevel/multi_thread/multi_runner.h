#pragma once

class MultiRunner 
{
public:
	// @param offset  全局参数起始索引（单机=0，MPI=task.start）
	static void MultiRun(int offset = 0);
};