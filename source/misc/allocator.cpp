// allocator.cpp
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#include <stdlib.h>

#include "defs.h"
#include "allocator.h"

#include "platform.h"

#if OS_WINDOWS
	#define WIN32_LEAN_AND_MEAN 1

	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>
#else
	#include <unistd.h>
	#include <sys/mman.h>
	#include <sys/ioctl.h>
#endif

namespace mem
{
	static void* _alloc(size_t bytes)
	{
		#if OS_WINDOWS
			auto ret = VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			if(ret == nullptr) _error_and_exit("failed to allocate %d bytes of memory (large page min: %d)\n", bytes, GetLargePageMinimum());

			return ret;
		#else
			auto ret = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			if(ret == nullptr || reinterpret_cast<uintptr_t>(ret) == static_cast<uintptr_t>(-1))
				_error_and_exit("failed to allocate %d bytes of memory\n", bytes);

			return ret;
		#endif
	}

	static void _dealloc(void* ptr, size_t bytes)
	{
		#if OS_WINDOWS
			VirtualFree(ptr, 0, MEM_RELEASE);
		#else
			munmap(ptr, bytes);
		#endif
	}













	static size_t allocated_count = 0;
	static size_t freed_count = 0;
	static size_t watermark = 0;

	void* allocate_memory(size_t bytes)
	{
		watermark += bytes;
		allocated_count += bytes;
		return _alloc(bytes);
	}

	void deallocate_memory(void* ptr, size_t bytes)
	{
		watermark -= bytes;
		freed_count += bytes;
		_dealloc(ptr, bytes);
	}

	void resetStats()
	{
		allocated_count = 0;
		freed_count = 0;
		watermark = 0;
	}

	size_t getAllocatedCount()
	{
		return allocated_count;
	}

	size_t getDeallocatedCount()
	{
		return freed_count;
	}

	size_t getWatermark()
	{
		return watermark;
	}
}
