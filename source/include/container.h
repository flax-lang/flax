// container.h
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include <vector>
#include <utility>

namespace util
{
	// only allows for insertions.
	// hopefully faster than FastVector.
	template <typename T, size_t ChunkSize = 512>
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
				auto nc = (T*) malloc(sizeof(T) * ChunkSize);
				memmove(nc, c, sizeof(T) * ChunkSize);
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

		FastInsertVector(FastInsertVector&& other)
		{
			// move.
			this->chunks = std::move(other.chunks);
			this->length = other.length;

			other.length = 0;
		}

		FastInsertVector& operator = (FastInsertVector&& other)
		{
			if(this != &other)
			{
				for(auto p : this->chunks)
					free(p);

				// move.
				this->chunks = std::move(other.chunks);
				this->length = other.length;

				other.length = 0;
			}

			return *this;
		}

		~FastInsertVector()
		{
			for(auto p : this->chunks)
				free(p);
		}


		T& operator [] (size_t index) const
		{
			size_t cind = index / ChunkSize;
			size_t offs = index % ChunkSize;

			iceAssert(cind < this->chunks.size());
			return *((T*) (this->chunks[cind] + offs));
		}

		T* getNextSlotAndIncrement()
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

		private:
		void makeNewChunk()
		{
			this->chunks.push_back((T*) malloc(sizeof(T) * ChunkSize));
		}

		// possibly use a faster implementation?? since we're just storing pointers idk if there's a point.
		size_t length;
		std::vector<T*> chunks;
	};



	template <typename T>
	struct MemoryPool
	{
		MemoryPool() { }
		~MemoryPool() { }

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

		MemoryPool(MemoryPool&& other)
		{
			this->storage = std::move(other.storage);
		}

		MemoryPool& operator = (MemoryPool&& other)
		{
			if(this != &other) this->storage = std::move(other.storage);
			return *this;
		}

		template <typename... Args>
		T* operator () (Args&&... args)
		{
			return this->construct(std::forward<Args>(args)...);
		}

		template <typename... Args>
		T* construct(Args&&... args)
		{
			return new (this->storage.getNextSlotAndIncrement()) T(std::forward<Args>(args)...);
		}


		private:
		FastInsertVector<T, 512> storage;
	};
}





















