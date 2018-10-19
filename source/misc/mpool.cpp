// mpool.cpp
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include "defs.h"
#include "mpool.h"

#include <unordered_set>

namespace util
{
	static std::unordered_set<MemoryPool_base*> pools;
	void addPool(MemoryPool_base* pool)
	{
		pools.insert(pool);
	}

	void clearAllPools()
	{
		for(auto pool : pools)
			pool->clear();
	}
}