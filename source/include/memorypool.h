// memorypool.h
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once

#include "container.h"

namespace util
{
	void addPool(MemoryPool_base* pool);
	void clearAllPools();


	template <typename T, typename... Args>
	T* pool(Args&&... args)
	{
		static MemoryPool<T, 1 << 9> _pool;
		addPool(&_pool);
		return _pool.construct(std::forward<Args>(args)...);
	}
}
