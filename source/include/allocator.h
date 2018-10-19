// allocator.h
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include <stdint.h>
#include <stddef.h>

namespace mem
{
	void* allocate_memory(size_t bytes);
	void deallocate_memory(void* ptr, size_t bytes);


	size_t getAllocatedCount();
	size_t getDeallocatedCount();
	size_t getWatermark();
	void resetStats();
}



