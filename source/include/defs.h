// defs.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <string>
#include <vector>
#include <experimental/string_view>


inline void _error_and_exit(const char* s, ...) __attribute__((noreturn));
inline void _error_and_exit(const char* s, ...)
{
	va_list ap;
	va_start(ap, s);

	// char* alloc = nullptr;
	// vasprintf(&alloc, s, ap);

	fprintf(stderr, "%s%s%s%s: \n", "\033[1m\033[31m", "Error", "\033[0m", "\033[1m");
	vfprintf(stderr, s, ap);
	fprintf(stderr, "%s\n", "\033[0m");

	// free(alloc);

	va_end(ap);
	abort();
}


#define __nothing
#define iceAssert(x)		((x) ? ((void) (0)) : _error_and_exit("Compiler assertion at %s:%d, cause:\n'%s' evaluated to false\n", __FILE__, __LINE__, #x))


#define TAB_WIDTH 4

namespace stx
{
	using string_view = std::experimental::string_view;
}

struct Identifier
{
	std::string name;
};

struct Location
{
	size_t fileID = 0;
	size_t line = 0;
	size_t col = 0;
	size_t len = 0;
};

enum class PrivacyLevel
{
	Invalid,

	Public,
	Private,
	Internal,
};

struct TypeConstraints_t
{
	std::vector<std::string> protocols;
	int pointerDegree = 0;

	bool operator == (const TypeConstraints_t& other) const
	{
		return this->protocols == other.protocols && this->pointerDegree == other.pointerDegree;
	}
};


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


enum class Operator
{
	Invalid,

	Add,
	Subtract,
	Multiply,
	Divide,
	Modulo,

	Assign,

	BitwiseOr,
	BitwiseAnd,
	BitwiseXor,

	LogicalOr,
	LogicalAnd,
	LogicalNot,

	ShiftLeft,
	ShiftRight,

	CompareEq,
	CompareNotEq,
	CompareGreater,
	CompareGreaterEq,
	CompareLess,
	CompareLessEq,

	Cast,
	DotOperator,

	BitwiseNot,
	Minus,
	Plus,

	AddressOf,
	Dereference,

	PlusEquals,
	MinusEquals,
	MultiplyEquals,
	DivideEquals,
	ModuloEquals,
	ShiftLeftEquals,
	ShiftRightEquals,
	BitwiseAndEquals,
	BitwiseOrEquals,
	BitwiseXorEquals,
};


std::string strprintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))


#define COLOUR_RESET			"\033[0m"
#define COLOUR_BLACK			"\033[30m"			// Black
#define COLOUR_RED				"\033[31m"			// Red
#define COLOUR_GREEN			"\033[32m"			// Green
#define COLOUR_YELLOW			"\033[33m"			// Yellow
#define COLOUR_BLUE				"\033[34m"			// Blue
#define COLOUR_MAGENTA			"\033[35m"			// Magenta
#define COLOUR_CYAN				"\033[36m"			// Cyan
#define COLOUR_WHITE			"\033[37m"			// White
#define COLOUR_BLACK_BOLD		"\033[1m"			// Bold Black
#define COLOUR_RED_BOLD			"\033[1m\033[31m"	// Bold Red
#define COLOUR_GREEN_BOLD		"\033[1m\033[32m"	// Bold Green
#define COLOUR_YELLOW_BOLD		"\033[1m\033[33m"	// Bold Yellow
#define COLOUR_BLUE_BOLD		"\033[1m\033[34m"	// Bold Blue
#define COLOUR_MAGENTA_BOLD		"\033[1m\033[35m"	// Bold Magenta
#define COLOUR_CYAN_BOLD		"\033[1m\033[36m"	// Bold Cyan
#define COLOUR_WHITE_BOLD		"\033[1m\033[37m"	// Bold White
#define COLOUR_GREY_BOLD		"\033[30;1m"		// Bold Grey


