// defs.h
// Copyright (c) 2014 - 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "precompile.h"

[[noreturn]] void doTheExit();

template <typename... Ts>
[[noreturn]] inline void _error_and_exit(const char* s, Ts... ts)
{
	tinyformat::format(std::cerr, s, ts...);
	doTheExit();
}


#define __nothing
#define iceAssert(x)		((x) ? ((void) (0)) : _error_and_exit("Compiler assertion at %s:%d, cause:\n'%s' evaluated to false\n", __FILE__, __LINE__, #x))

#define TAB_WIDTH	4


namespace util
{
	#ifndef STRING_VIEW_TYPE
		#error "what?"
	#endif

	#if STRING_VIEW_TYPE == 0
		using string_view = std::string_view;
	#elif STRING_VIEW_TYPE == 1
		using string_view = std::experimental::string_view;
	#elif STRING_VIEW_TYPE == 2
		using string_view = stx::string_view;
	#else
		#error "No string_view, or unknown type"
	#endif

	inline std::string to_string(const string_view& sv)
	{
		return std::string(sv.data(), sv.length());
	}
}

namespace fir
{
	struct Type;
	struct Value;
}

enum class IdKind
{
	Invalid,
	Name,
	Function,

	Type,
};

struct Identifier
{
	Identifier() : name(""), kind(IdKind::Invalid) { }
	Identifier(std::string n, IdKind k) : name(n), kind(k) { }

	std::string name;
	std::vector<std::string> scope;
	std::vector<fir::Type*> params;

	IdKind kind;

	std::string str() const;
	std::string mangled() const;
	std::string mangledName() const;

	bool operator == (const Identifier& other) const { return other.str() == this->str(); }
	bool operator != (const Identifier& other) const { return !(other == *this); }
};


struct CGResult
{
	enum class VK
	{
		Invalid,
		LValue,		// lvalue, as in c/c++
		RValue,		// rvalue, as in c/c++
		LitRValue,	// literal rvalue, simplifies refcounting a little I guess

		Break,
		Continue,
	};

	CGResult() : CGResult(0) { }
	explicit CGResult(fir::Value* v) : value(v), pointer(0), kind(VK::RValue) { }
	explicit CGResult(fir::Value* v, fir::Value* p) : value(v), pointer(p), kind(VK::RValue) { }
	explicit CGResult(fir::Value* v, fir::Value* p, VK k) : value(v), pointer(p), kind(k) { }

	fir::Value* value = 0;
	fir::Value* pointer = 0;

	VK kind = VK::Invalid;

	CGResult& operator = (const CGResult& other)
	{
		if(this == &other) return *this;

		this->kind = other.kind;
		this->value = other.value;
		this->pointer = other.pointer;

		return *this;
	}
};

namespace std
{
	template<>
	struct hash<Identifier>
	{
		std::size_t operator()(const Identifier& k) const
		{
			using std::size_t;
			using std::hash;
			using std::string;

			// Compute individual hash values for first,
			// second and third and combine them using XOR
			// and bit shifting:

			// return ((hash<string>()(k.name) ^ (hash<std::vector<std::string>>()(k.scope) << 1)) >> 1) ^ (hash<int>()(k.third) << 1);
			return hash<string>()(k.str());
		}
	};
}

struct Location
{
	size_t fileID = 0;
	size_t line = 0;
	size_t col = 0;
	size_t len = 0;

	bool operator == (const Location& other) const
	{
		return this->col == other.col && this->line == other.line && this->len == other.len && this->fileID == other.fileID;
	}

	bool operator != (const Location& other) const
	{
		return !(*this == other);
	}

	std::string toString() const;
};

struct Locatable
{
	Locatable(const Location& l) : loc(l) { }

	Location loc;
};

enum class VisibilityLevel
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
	template <typename T, class UnaryOp, typename K = typename std::result_of<UnaryOp(T)>::type>
	std::vector<K> map(std::vector<T> input, UnaryOp fn)
	{
		std::vector<K> ret;
		for(auto i : input)
			ret.push_back(fn(i));

		return ret;
	}

	template <typename T, class Predicate>
	std::vector<T> filter(std::vector<T> input, Predicate cond)
	{
		std::vector<T> ret;
		for(const auto& i : input)
			if(cond(i))
				ret.push_back(i);

		return ret;
	}


	inline std::string serialiseScope(const std::vector<std::string>& scope)
	{
		std::string ret;
		for(const std::string& s : scope)
			ret += s + ".";

		if(!ret.empty() && ret.back() == '.')
			ret.pop_back();

		return ret;
	}



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

std::string operatorToString(const Operator& op);

bool isAssignOp(Operator op);
bool isBitwiseOp(Operator op);
bool isCompareOp(Operator op);
Operator getNonAssignOp(Operator op);



template <typename... Ts>
std::string strprintf(const char* fmt, Ts... ts)
{
	return tinyformat::format(fmt, ts...);
}


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







// defer implementation
// credit: gingerBill
// shamelessly stolen from http://www.gingerbill.org/article/defer-in-cpp.html

namespace __dontlook
{
	template <typename F>
	struct privDefer
	{
		F f;
		privDefer(F f) : f(f) { }
		~privDefer() { f(); }
	};

	template <typename F>
	privDefer<F> defer_func(F f)
	{
		return privDefer<F>(f);
	}
}

#define DEFER_1(x, y)	x##y
#define DEFER_2(x, y)	DEFER_1(x, y)
#define DEFER_3(x)		DEFER_2(x, __COUNTER__)
#define defer(code)		auto DEFER_3(_defer_) = __dontlook::defer_func([&](){code;})







