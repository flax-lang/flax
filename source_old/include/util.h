// util.h
// Copyright (c) 2014 - 2016, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


namespace util
{
	template <typename T>
	class FastVector
	{
		public:
		FastVector()
		{
			this->array = 0;
			this->length = 0;
			this->capacity = 0;
		}
		FastVector(size_t initSize)
		{
			this->array = (T*) malloc(initSize * sizeof(T));
			this->capacity = initSize;
			this->length = 0;
		}

		FastVector(const FastVector& other)
		{
			this->array = (T*) malloc(other.capacity * sizeof(T));
			memmove(this->array, other.array, other.length * sizeof(T));

			this->capacity = other.capacity;
			this->length = other.length;
		}

		FastVector& operator = (const FastVector& other)
		{
			this->array = (T*) malloc(other.capacity * sizeof(T));
			memmove(this->array, other.array, other.length * sizeof(T));

			this->capacity = other.capacity;
			this->length = other.length;

			return *this;
		}

		FastVector(FastVector&& other)
		{
			// move.
			this->array = other.array;
			this->length = other.length;
			this->capacity = other.capacity;

			other.array = 0;
			other.length = 0;
			other.capacity = 0;
		}

		FastVector& operator = (FastVector&& other)
		{
			if(this != &other)
			{
				if(this->array)
					free(this->array);

				// move.
				this->array = other.array;
				this->length = other.length;
				this->capacity = other.capacity;

				other.array = 0;
				other.length = 0;
				other.capacity = 0;
			}

			return *this;
		}

		~FastVector()
		{
			if(this->array != 0)
				free(this->array);
		}

		size_t size() const
		{
			return this->length;
		}

		T& operator[] (size_t index) const
		{
			return this->array[index];
		}

		T* getEmptySlotPtrAndAppend()
		{
			this->autoResize();

			this->length++;
			return &this->array[this->length - 1];
		}

		void autoResize()
		{
			if(this->length == this->capacity)
			{
				if(this->capacity == 0)
					this->capacity = 64;

				this->array = (T*) realloc(this->array, this->capacity * 2 * sizeof(T));

				iceAssert(this->array);
				this->capacity *= 2;
			}
		}

		private:
		T* array = 0;
		size_t capacity;
		size_t length;
	};
}
