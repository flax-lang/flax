// mpool.h
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once

#include "container.h"

namespace util
{
	template <typename T, typename... Args>
	T* pool(Args&&... args)
	{
		static MemoryPool<T> _pool;
		return _pool.construct(std::forward<Args>(args)...);
	}
}