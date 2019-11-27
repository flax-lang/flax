// container.h
// Copyright (c) 2017, zhiayang
// Licensed under the Apache License Version 2.0.

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#include <vector>
#include <utility>

#include "allocator.h"

namespace util
{
	// only allows for insertions.
	// hopefully faster than FastVector.
	template <typename ValueType, size_t ChunkSize = 256>
	struct FastInsertVector
	{
		FastInsertVector()
		{
			this->length = 0;
		}

		FastInsertVector(const FastInsertVector& other)
		{
			for(auto c : other.chunks)
			{
				auto nc = static_cast<ValueType*>(mem::allocate_memory(sizeof(ValueType) * ChunkSize));
				memmove(nc, c, sizeof(ValueType) * ChunkSize);
				this->chunks.push_back(nc);
			}

			this->length = other.length;
		}

		FastInsertVector& operator = (const FastInsertVector& other)
		{
			auto copy = other;
			std::swap(copy, *this);
			return *this;
		}

		FastInsertVector(FastInsertVector&& other) noexcept
		{
			// move.
			this->chunks = std::move(other.chunks);
			this->length = other.length;

			other.length = 0;
		}

		FastInsertVector& operator = (FastInsertVector&& other) noexcept
		{
			if(this != &other)
			{
				for(auto p : this->chunks)
					mem::deallocate_memory(p, sizeof(ValueType) * ChunkSize);

				// move.
				this->chunks = std::move(other.chunks);
				this->length = other.length;

				other.length = 0;
			}

			return *this;
		}

		~FastInsertVector()
		{
			this->clear();
		}


		ValueType& operator [] (size_t index) const
		{
			size_t cind = index / ChunkSize;
			size_t offs = index % ChunkSize;

			assert(cind < this->chunks.size());
			return *(static_cast<ValueType*>(this->chunks[cind] + offs));
		}

		ValueType* getNextSlotAndIncrement()
		{
			size_t addr = this->length;
			if(addr >= (ChunkSize * this->chunks.size()))
				makeNewChunk();

			this->length++;
			auto ret = (chunks.back() + (addr % ChunkSize));

			return ret;
		}

		size_t size() const
		{
			return this->length;
		}

		void clear()
		{
			size_t i = 0;
			for(auto c : this->chunks)
			{
				for(size_t k = 0; k < ChunkSize && i < this->length; k++, i++)
					(c + k)->~ValueType();

				mem::deallocate_memory(c, sizeof(ValueType) * ChunkSize);
			}

			this->length = 0;
			this->chunks.clear();
		}

		private:
		void makeNewChunk()
		{
			this->chunks.push_back(static_cast<ValueType*>(mem::allocate_memory(sizeof(ValueType) * ChunkSize)));
		}

		// possibly use a faster implementation?? since we're just storing pointers idk if there's a point.
		size_t length;
		std::vector<ValueType*> chunks;
	};

	struct MemoryPool_base
	{
		virtual void clear() = 0;
		virtual ~MemoryPool_base() { }
	};

	template <typename ValueType, size_t ChunkSize = 256>
	struct MemoryPool : MemoryPool_base
	{
		MemoryPool() { }
		~MemoryPool() { this->storage.clear(); }

		MemoryPool(const MemoryPool& other)
		{
			this->storage = other.storage;
		}

		MemoryPool& operator = (const MemoryPool& other)
		{
			auto copy = other;
			std::swap(copy, *this);
			return *this;
		}

		MemoryPool(MemoryPool&& other) noexcept
		{
			this->storage = std::move(other.storage);
		}

		MemoryPool& operator = (MemoryPool&& other) noexcept
		{
			if(this != &other) this->storage = std::move(other.storage);
			return *this;
		}

		template <typename... Args>
		ValueType* operator () (Args&&... args)
		{
			return this->construct(std::forward<Args>(args)...);
		}

		template <typename... Args>
		ValueType* construct(Args&&... args)
		{
			return new (this->storage.getNextSlotAndIncrement()) ValueType(std::forward<Args>(args)...);
		}

		virtual void clear() override
		{
			this->storage.clear();
		}

		private:
		FastInsertVector<ValueType, ChunkSize> storage;
	};
}





















